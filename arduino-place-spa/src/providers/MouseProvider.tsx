import {
  createContext,
  PropsWithChildren,
  useCallback,
  useContext,
  useEffect,
  useState,
} from "react";

export type MouseState = {
  leftHeld: boolean;
};

const MouseContext = createContext<MouseState>({ leftHeld: false });

/**
 * Provides details regarding the state of the mouse.
 */
export default function MouseProvider({ children }: PropsWithChildren) {
  const [leftHeld, setLeftHeld] = useState(false);

  const handleLeftDown = useCallback(() => setLeftHeld(true), []);
  const handleLeftUp = useCallback(() => setLeftHeld(false), []);

  useEffect(() => {
    document.addEventListener("mousedown", handleLeftDown);
    document.addEventListener("mouseup", handleLeftUp);
    document.addEventListener("touchstart", handleLeftDown);
    document.addEventListener("touchend", handleLeftUp);

    return () => {
      document.removeEventListener("mousedown", () => handleLeftDown);
      document.removeEventListener("mouseup", () => handleLeftUp);
      document.removeEventListener("touchstart", () => handleLeftDown);
      document.removeEventListener("touchend", () => handleLeftUp);
    };
  }, [handleLeftDown, handleLeftUp]);

  return (
    <MouseContext.Provider value={{ leftHeld }}>
      {children}
    </MouseContext.Provider>
  );
}

// eslint-disable-next-line react-refresh/only-export-components
export const useMouse = () => useContext(MouseContext);
