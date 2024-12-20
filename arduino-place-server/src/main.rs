use arduino_place_server::{
    led_array::{persisted::PersistedLedArray, Color, LedArray, LED_MAX_INDEX},
    ws::create_on_upgrade_callback,
    BroadcastMessage,
};
use axum::{
    extract::{FromRef, Path, State, WebSocketUpgrade},
    response::IntoResponse,
    routing::get,
    Form, Json, Router,
};
use axum_valid::Valid;
use serde::Deserialize;
use shuttle_persist::PersistInstance;
use tokio::sync::broadcast;
use tower_http::{
    cors::{AllowMethods, AllowOrigin, CorsLayer},
    services::{ServeDir, ServeFile},
    trace::TraceLayer,
};
use validator::Validate;

#[derive(Clone)]
struct AppState {
    led_array: PersistedLedArray,
    tx: broadcast::Sender<BroadcastMessage>,
}

impl FromRef<AppState> for PersistedLedArray {
    fn from_ref(app_state: &AppState) -> Self { app_state.led_array.clone() }
}

impl FromRef<AppState> for broadcast::Sender<BroadcastMessage> {
    fn from_ref(app_state: &AppState) -> Self { app_state.tx.clone() }
}

async fn get_ws(
    State(led_array): State<PersistedLedArray>,
    State(tx): State<broadcast::Sender<BroadcastMessage>>,
    ws: WebSocketUpgrade,
) -> impl IntoResponse {
    let rx = tx.subscribe();

    ws.on_upgrade(create_on_upgrade_callback(led_array, rx))
}

#[derive(Deserialize, Validate)]
struct LedParams {
    #[validate(range(min = 0, max = LED_MAX_INDEX))]
    index: usize,
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
    State(tx): State<broadcast::Sender<BroadcastMessage>>,
    Valid(Path(LedParams { index })): Valid<Path<LedParams>>,
    Form(color): Form<Color>,
) {
    led_array.set_led(index, color).await.expect("in bounds");
    let _ = tx.send(BroadcastMessage::LedUpdated { index, color });
}

#[shuttle_runtime::main]
async fn main(#[shuttle_persist::Persist] persist: PersistInstance) -> shuttle_axum::ShuttleAxum {
    let led_array = PersistedLedArray::new(persist);
    let tx = broadcast::Sender::<BroadcastMessage>::new(16);
    let app_state = AppState { led_array, tx };

    let cors = CorsLayer::new()
        .allow_methods(AllowMethods::any())
        .allow_origin(AllowOrigin::any());

    let router = Router::new()
        .route("/ws", get(get_ws))
        .route("/leds", get(get_leds))
        .route("/leds/:index", get(get_led).post(post_led))
        .with_state(app_state)
        .nest_service("/", ServeFile::new("static/index.html"))
        .nest_service("/assets", ServeDir::new("static/assets"))
        .layer(TraceLayer::new_for_http())
        .layer(cors);

    Ok(router.into())
}
