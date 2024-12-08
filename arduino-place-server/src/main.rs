use std::sync::Arc;

use arduino_place_server::led_array::{persisted::PersistedLedArray, Color, LedArray, LED_COUNT};
use axum::{
    extract::{
        ws::{Message, WebSocket},
        FromRef, Path, State, WebSocketUpgrade,
    },
    response::IntoResponse,
    routing::get,
    Json, Router,
};
use axum_valid::Valid;
use futures::{stream::StreamExt, SinkExt};
use serde::Deserialize;
use shuttle_persist::PersistInstance;
use tokio::sync::{broadcast, Mutex};
use tower_http::trace::TraceLayer;
use validator::Validate;

#[derive(Deserialize, Validate)]
struct LedParams {
    #[validate(range(min = 0, max = LED_COUNT))]
    index: usize,
}

#[derive(Clone, Copy)]
struct SyncOneBroadcast {
    index: usize,
    color: Color,
}

impl From<SyncOneBroadcast> for Vec<u8> {
    fn from(value: SyncOneBroadcast) -> Self {
        [
            value.index as u8,
            value.color.red,
            value.color.green,
            value.color.blue,
        ]
        .into()
    }
}

#[derive(Clone)]
struct AppState {
    led_array: PersistedLedArray,
    tx: broadcast::Sender<SyncOneBroadcast>,
}

impl FromRef<AppState> for PersistedLedArray {
    fn from_ref(app_state: &AppState) -> Self { app_state.led_array.clone() }
}

impl FromRef<AppState> for broadcast::Sender<SyncOneBroadcast> {
    fn from_ref(app_state: &AppState) -> Self { app_state.tx.clone() }
}

// TODO: organize ws ocde

async fn get_ws(
    State(led_array): State<PersistedLedArray>,
    State(tx): State<broadcast::Sender<SyncOneBroadcast>>,
    ws: WebSocketUpgrade,
) -> impl IntoResponse {
    dbg!("GET");
    let rx = tx.subscribe();

    ws.on_upgrade(move |socket| on_upgrade_callback(led_array, rx, socket))
}

async fn on_upgrade_callback(
    led_array: PersistedLedArray,
    mut rx: broadcast::Receiver<SyncOneBroadcast>,
    socket: WebSocket,
) {
    let (socket_tx, mut socket_rx) = socket.split();
    let socket_tx = Arc::new(Mutex::new(socket_tx));

    let socket_rx_task_socket_tx = socket_tx.clone();
    let socket_rx_task_led_array = led_array.clone();
    let mut socket_rx_task = tokio::task::spawn(async move {
        let mut waiting_on_first_message = true;
        loop {
            match socket_rx.next().await {
                Some(Ok(message)) => {
                    match message {
                        Message::Ping(vec) => {
                            if let Err(err) = socket_rx_task_socket_tx
                                .lock()
                                .await
                                .send(Message::Pong(vec))
                                .await
                            {
                                dbg!(err);
                                break;
                            }
                        }
                        Message::Close(_) => break,
                        _ => (),
                    };

                    if waiting_on_first_message {
                        // Contains repeating colors in the form of R, G, B, R, G, B, ...
                        // where each color channel is a byte.
                        let payload = socket_rx_task_led_array.clone().get_leds().await.into();

                        if let Err(err) = socket_rx_task_socket_tx
                            .lock()
                            .await
                            .send(Message::Binary(payload))
                            .await
                        {
                            dbg!(err);
                            break;
                        };
                    }

                    waiting_on_first_message = false;
                }
                Some(Err(err)) => {
                    dbg!(err);
                    break;
                }
                None => {
                    dbg!("Connection closed");
                    break;
                }
            }
        }
    });

    let mut socket_tx_task = tokio::task::spawn(async move {
        loop {
            match rx.recv().await {
                Ok(sync_one) => {
                    if socket_tx
                        .lock()
                        .await
                        .send(Message::Binary(sync_one.into()))
                        .await
                        .is_err()
                    {
                        break;
                    };
                }
                Err(err) => match err {
                    broadcast::error::RecvError::Closed => break,
                    broadcast::error::RecvError::Lagged(_) => {
                        if socket_tx
                            .lock()
                            .await
                            .send(Message::Binary(led_array.clone().get_leds().await.into()))
                            .await
                            .is_err()
                        {
                            break;
                        } else {
                            rx = rx.resubscribe();
                        };
                    }
                },
            }
        }
    });

    tokio::select! {
        _ = &mut socket_tx_task => socket_rx_task.abort(),
        _ = &mut socket_rx_task => socket_tx_task.abort(),
    }
}

async fn get_leds(State(led_array): State<PersistedLedArray>) -> Json<LedArray> {
    Json(led_array.get_leds().await)
}

async fn get_led(
    State(led_array): State<PersistedLedArray>,
    params: Valid<Path<LedParams>>,
) -> Json<Color> {
    Json(led_array.get_led(params.index).await.expect("in bounds"))
}

async fn post_led(
    State(led_array): State<PersistedLedArray>,
    State(tx): State<broadcast::Sender<SyncOneBroadcast>>,
    Valid(Path(LedParams { index })): Valid<Path<LedParams>>,
    Json(color): Json<Color>,
) {
    led_array.set_led(index, color).await.expect("in bounds");
    let _ = tx.send(SyncOneBroadcast { index, color });
}

#[shuttle_runtime::main]
async fn main(#[shuttle_persist::Persist] persist: PersistInstance) -> shuttle_axum::ShuttleAxum {
    let led_array = PersistedLedArray::new(persist);
    let tx = broadcast::Sender::<SyncOneBroadcast>::new(16);
    let app_state = AppState { led_array, tx };

    let router = Router::new()
        .route("/ws", get(get_ws))
        .route("/leds", get(get_leds))
        .route("/leds/:index", get(get_led).post(post_led))
        .layer(TraceLayer::new_for_http())
        .with_state(app_state);

    Ok(router.into())
}
