import { type RgbColor, RgbColorPicker } from "react-colorful";
import { useLeds } from "./hooks/useLeds";
import { useEasel } from "./hooks/useEasel";
import { useMouse } from "./providers/MouseProvider";
import { useCallback, useEffect, useRef, useState } from "react";
import { getLeds } from "./api/rest";

function rgb(r: number, g: number, b: number): RgbColor {
  return {
    r,
    g,
    b,
  };
}

const randint = (min: number, max: number) =>
  min + Math.floor(Math.random() * (max - min));

const treeConfig = [19, 17, 15, 13, 11, 9, 7, 5, 3, 1];
const rise = 1.0 / treeConfig.length;

const ledPositions = treeConfig.flatMap((ledsPerRow, rowIndex) => {
  const rowProgress = rowIndex / treeConfig.length;
  const rightTriangleWidth = 0.5 * (1.0 - rowProgress);
  const xmin = 0.5 - rightTriangleWidth;
  const run = 1.0 - rise * (rowIndex + 1);

  return [...Array(ledsPerRow).keys()].map((index) => {
    const progress = index / ledsPerRow;
    const x = xmin + run * progress;
    const y =
      -rise * progress + (treeConfig.length - rowIndex) / treeConfig.length;

    return {
      x,
      y,
    };
  });
});

function LedButton({
  x,
  y,
  color,
  onPaint,
}: {
  x: number;
  y: number;
  color: RgbColor;
  onPaint: () => void;
}) {
  const { leftHeld } = useMouse();
  const buttonRef = useRef<HTMLButtonElement>(null);

  const handleTouchMove = useCallback(
    (event: TouchEvent) => {
      if (leftHeld) {
        const touch = event.touches[0];
        const elementUnderTouch = document.elementFromPoint(
          touch.clientX,
          touch.clientY
        );

        if (elementUnderTouch === buttonRef.current) {
          onPaint();
        }
      }
    },
    [leftHeld, onPaint]
  );

  useEffect(() => {
    document.addEventListener("touchmove", handleTouchMove);
    return () => document.removeEventListener("touchmove", handleTouchMove);
  });

  return (
    <button
      ref={buttonRef}
      className="btn btn-circle btn-xs border-none absolute"
      style={{
        top: `${y * 100}%`,
        left: `${x * 100}%`,
        transform: "translate(50%, -100%)",
        backgroundColor: `rgb(${color.r}, ${color.g}, ${color.b})`,
      }}
      onMouseEnter={() => {
        if (leftHeld) {
          onPaint();
        }
      }}
      onPointerEnter={() => {
        if (leftHeld) {
          onPaint();
        }
      }}
      onClick={onPaint}
    ></button>
  );
}

function TreeLeds({
  initialLeds,
  paintColor,
}: {
  initialLeds: RgbColor[];
  paintColor: RgbColor;
}) {
  const { ledColors, paintLed } = useLeds(initialLeds);

  return (
    <>
      {ledPositions.map(({ x, y }, index) => (
        <LedButton
          key={index}
          x={x}
          y={y}
          color={ledColors[index]}
          onPaint={() => paintLed(index, paintColor)}
        />
      ))}
    </>
  );
}

function App() {
  const {
    easelColors,
    activeEaselColorIndex,
    switchActiveEaselIndex,
    activeEaselColor,
    setActiveEaselColor,
  } = useEasel(() =>
    [...Array(9).keys()].map(() =>
      rgb(randint(0, 255), randint(0, 255), randint(0, 255))
    )
  );

  const [initialLeds, setInitialLeds] = useState<RgbColor[]>();
  useEffect(() => {
    (async () => {
      setInitialLeds(await getLeds());
    })();
  }, []);

  return (
    <div className="min-h-screen flex flex-col fixed sm:relative w-full">
      <div className="navbar bg-base-100">
        <div className="navbar-start"></div>
        <h1 className="navbar-center text-4xl">Arduino Place</h1>
        <div className="navbar-end"></div>
      </div>
      <div className="flex flex-col xl:flex-row xl:justify-center flex-1 justify-evenly space-y-8 p-8 items-center">
        <div className="relative aspect-square w-full max-w-lg">
          <div className="w-full h-full bg-green-600 clip-path-polygon-[0%_100%,50%_0%,_100%_100%]"></div>
          {initialLeds && (
            <TreeLeds initialLeds={initialLeds} paintColor={activeEaselColor} />
          )}
        </div>
        <div className="flex flex-row justify-center space-x-8">
          <div className="grid grid-cols-3 gap-4">
            {easelColors.map((color, index) => (
              <button
                key={index}
                className={`btn-circle border-0 hover:scale-125 transition-transform ${
                  index == activeEaselColorIndex
                    ? " scale-110 outline outline-white"
                    : ""
                }`}
                style={{
                  backgroundColor: `rgb(${color.r}, ${color.g}, ${color.b})`,
                }}
                onClick={() => switchActiveEaselIndex(index)}
              ></button>
            ))}
          </div>
          <RgbColorPicker
            color={activeEaselColor}
            onChange={setActiveEaselColor}
          />
        </div>
      </div>
    </div>
  );
}

export default App;
