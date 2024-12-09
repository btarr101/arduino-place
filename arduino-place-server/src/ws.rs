use std::{future::Future, pin::Pin, sync::Arc};

use axum::extract::ws::{Message, WebSocket};
use futures::{stream::SplitSink, SinkExt, StreamExt};
use tokio::sync::{broadcast, Mutex};

use crate::{
    led_array::{persisted::PersistedLedArray, Color, LedArray},
    BroadcastMessage,
};

#[derive(Clone)]
pub enum WsPayload {
    SyncOne { index: usize, color: Color },
    SyncAll(Box<LedArray>),
}

impl From<WsPayload> for Vec<u8> {
    fn from(payload: WsPayload) -> Self {
        match payload {
            WsPayload::SyncOne { index, color } => {
                [index as u8, color.red, color.green, color.blue].into()
            }
            WsPayload::SyncAll(led_array) => (*led_array).into(),
        }
    }
}

trait WsPayloadSender {
    async fn send_ws_payload(&self, payload: WsPayload) -> Result<(), axum::Error>;
}

impl WsPayloadSender for Arc<Mutex<SplitSink<WebSocket, Message>>> {
    async fn send_ws_payload(&self, payload: WsPayload) -> Result<(), axum::Error> {
        self.lock()
            .await
            .send(Message::Binary(payload.into()))
            .await
    }
}

pub fn create_on_upgrade_callback(
    led_array: PersistedLedArray,
    mut rx: broadcast::Receiver<BroadcastMessage>,
) -> impl (FnOnce(WebSocket) -> Pin<Box<dyn Future<Output = ()> + Send + 'static>>) + Send + 'static
{
    |socket: WebSocket| {
        Box::pin(async move {
            let (socket_tx, mut socket_rx) = socket.split();
            let socket_tx = Arc::new(Mutex::new(socket_tx));

            let socket_rx_task_socket_tx = socket_tx.clone();
            let socket_rx_task_led_array = led_array.clone();
            let mut socket_rx_task = tokio::task::spawn(async move {
                let mut waiting_on_first_message = true;
                loop {
                    match socket_rx.next().await {
                        Some(Ok(message)) => {
                            if let Message::Close(close_frame) = message {
                                tracing::info!("Close message recieved: {:?}", close_frame);
                                break;
                            };

                            if waiting_on_first_message {
                                let payload = WsPayload::SyncAll(Box::new(
                                    socket_rx_task_led_array.clone().get_leds().await,
                                ));
                                if let Err(err) =
                                    socket_rx_task_socket_tx.send_ws_payload(payload).await
                                {
                                    tracing::error!("Error sending initial payload: {}", err);
                                    break;
                                }

                                tracing::info!("Sent initial payload!");
                                waiting_on_first_message = false;
                            }
                        }
                        Some(Err(err)) => {
                            tracing::error!("Error when recieving message: {}", err);
                            break;
                        }
                        None => {
                            tracing::error!("Connection closed");
                            break;
                        }
                    }
                }

                tracing::info!("Closing websocket from rx task");
            });

            let mut socket_tx_task = tokio::task::spawn(async move {
                loop {
                    match rx.recv().await {
                        Ok(message) => {
                            let payload = match message {
                                BroadcastMessage::LedUpdated { index, color } => {
                                    WsPayload::SyncOne { index, color }
                                }
                            };

                            if let Err(err) = socket_tx.send_ws_payload(payload).await {
                                tracing::error!("Failed to send payload: {}", err);
                                break;
                            }
                        }
                        Err(err) => match err {
                            broadcast::error::RecvError::Closed => break,
                            broadcast::error::RecvError::Lagged(_) => {
                                tracing::warn!("Broadcast reciever lagged");

                                let payload = WsPayload::SyncAll(Box::new(
                                    led_array.clone().get_leds().await,
                                ));

                                if let Err(err) = socket_tx.send_ws_payload(payload).await {
                                    tracing::error!("Failed to send lag payload: {}", err);
                                    break;
                                }

                                rx = rx.resubscribe();
                            }
                        },
                    }
                }

                tracing::info!("Closing websocket from tx task");
            });

            tokio::select! {
                _ = &mut socket_tx_task => socket_rx_task.abort(),
                _ = &mut socket_rx_task => socket_tx_task.abort(),
            };
        })
    }
}
