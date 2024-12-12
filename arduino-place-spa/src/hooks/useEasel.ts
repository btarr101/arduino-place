import { useCallback, useMemo, useState } from "react";
import { RgbColor } from "react-colorful";

export type EaselContext = {
  readonly easelColors: RgbColor[];
  activeEaselColorIndex: number;
  switchActiveEaselIndex: (newIndex: number) => void;
  activeEaselColor: RgbColor;
  setActiveEaselColor: (newColor: RgbColor) => void;
};

/**
 * Hook to manage state of Leds.
 *
 * @param ledCount The number of leds.
 * @returns
 */
export function useEasel(getInitialColors: () => RgbColor[]): EaselContext {
	const [selectedColorIndex, setSelectedColorIndex] = useState(0);
  const [easelColors, setEaselColors] = useState(getInitialColors);

  const activeEaselColorIndex = selectedColorIndex;
  const switchActiveEaselIndex = setSelectedColorIndex;

  const activeEaselColor = useMemo(() => easelColors[activeEaselColorIndex], [easelColors, activeEaselColorIndex]);
  const setActiveEaselColor = useCallback((newColor: RgbColor) => {
    const newColors = easelColors.map((color, index) =>
      index === selectedColorIndex ? newColor : color
    );
    setEaselColors(newColors);
  }, [easelColors, selectedColorIndex])

  return {
    easelColors,
    activeEaselColorIndex,
    switchActiveEaselIndex,
    activeEaselColor,
    setActiveEaselColor,
  };
}
