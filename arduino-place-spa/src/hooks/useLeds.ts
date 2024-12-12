import { useCallback, useEffect, useState } from "react";
import { RgbColor } from "react-colorful";
import { postLed } from "../api/rest";
import useWebSocket, { ReadyState } from "react-use-websocket";

const wsUrl = "wss://arduino-place-server-lrpx.shuttle.app/ws";

export type LedContext = {
  readonly ledColors: RgbColor[];
  paintLed: (ledIndex: number, color: RgbColor) => void;
};

/**
 * Hook to manage state of Leds.
 *
 * @param ledCount The number of leds.
 * @returns
 */
export function useLeds(initialLeds: RgbColor[]): LedContext {
  const [ledColors, setLedColors] = useState(initialLeds);
  const paintLed = useCallback(
    (ledIndex: number, color: RgbColor) => {
      const currentColor = ledColors[ledIndex];
      if (currentColor.r === color.r && currentColor.g === color.g && currentColor.b === color.b) {
        return;
      }

      const newLedColors = ledColors.map((oldColor, index) =>
        index === ledIndex ? color : oldColor
      );
      setLedColors(newLedColors);
      postLed(ledIndex, color);
    },
    [ledColors]
  );

  const { sendMessage, lastMessage, readyState } = useWebSocket(wsUrl);

  useEffect(() => {
    if (readyState === ReadyState.OPEN) {
      sendMessage("Connected")
    }
  }, [readyState, sendMessage]);
  useEffect(() => {
    if (lastMessage) {
      (async () => {
        const buffer = await (lastMessage.data as Blob).arrayBuffer();
        const array = new Uint8Array(buffer); 
        if (array.length === 4) {
          const index = array[0];
          const r = array[1];
          const g = array[2];
          const b = array[3];
    
          const color = { r, g, b};
          paintLed(index, color);
        }
      })();
    }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [lastMessage])

  return {
    ledColors,
    paintLed,
  };
}
