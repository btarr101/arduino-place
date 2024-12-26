# Arduino Place Server

Backend server application that hosts

- The frontend application
- REST API for getting and controlling LEDs
- Websocket connection for recieving LED updates

## Running

This backend uses https://www.shuttle.dev/, so you will need https://docs.shuttle.dev/getting-started/installation#pre-built-binary.

### Locally

```bash
cargo shuttle run
```

### Deployed

```bash
cargo shuttle deploy
```

However you will need to configure the shuttle project id. Read more here: https://docs.shuttle.dev/guides/cli#start-and-deploy-a-project
