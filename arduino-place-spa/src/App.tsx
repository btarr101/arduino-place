import { useCallback, useState } from "react";
import { type RgbColor, RgbColorPicker } from "react-colorful";

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
  return (
    <button
      className="btn btn-circle btn-xs border-none absolute"
      style={{
        top: `${y * 100}%`,
        left: `${x * 100}%`,
        transform: "translate(50%, -100%)",
        backgroundColor: `rgb(${color.r}, ${color.g}, ${color.b})`,
      }}
      onClick={onPaint}
    ></button>
  );
}

function App() {
  const [selectedColorIndex, setSelectedColorIndex] = useState(0);
  const [colors, setColors] = useState(() =>
    [...Array(9).keys()].map(() =>
      rgb(randint(0, 255), randint(0, 255), randint(0, 255))
    )
  );

  const [ledColors, setLedColors] = useState(() =>
    [...Array(treeConfig.reduce((a, b) => a + b, 0))].map(() => ({
      r: 0,
      b: 0,
      g: 0,
    }))
  );

  const onLedPainted = useCallback(
    (ledIndex: number) => {
      const newLedColors = ledColors.map((color, index) =>
        index === ledIndex ? colors[selectedColorIndex] : color
      );
      setLedColors(newLedColors);
    },
    [colors, ledColors, selectedColorIndex]
  );

  return (
    <div className="max-h-screen h-screen flex flex-col">
      <div className="navbar bg-base-100">
        <div className="navbar-start"></div>
        <h1 className="navbar-center text-4xl">Arduino Place</h1>
        <div className="navbar-end"></div>
      </div>
      <div className="flex flex-col flex-1 justify-evenly space-y-8 p-8 items-center">
        <div className="relative aspect-square w-full max-w-lg">
          <div className="w-full h-full bg-green-600 clip-path-polygon-[0%_100%,50%_0%,_100%_100%]"></div>
          {ledPositions.map(({ x, y }, index) => (
            <LedButton
              key={index}
              x={x}
              y={y}
              color={ledColors[index]}
              onPaint={() => onLedPainted(index)}
            />
          ))}
        </div>
        <div className="flex flex-row justify-center space-x-8 w-full">
          <div className="grid grid-cols-3 gap-4">
            {colors.map((color, index) => (
              <button
                key={index}
                className={`btn-circle border-0 hover:scale-125 transition-transform ${
                  index == selectedColorIndex
                    ? " scale-110 outline outline-white"
                    : ""
                }`}
                style={{
                  backgroundColor: `rgb(${color.r}, ${color.g}, ${color.b})`,
                }}
                onClick={() => setSelectedColorIndex(index)}
              ></button>
            ))}
          </div>
          <RgbColorPicker
            color={colors[selectedColorIndex]}
            onChange={(newColor) => {
              const newColors = colors.map((color, index) =>
                index === selectedColorIndex ? newColor : color
              );
              setColors(newColors);
            }}
          />
        </div>
      </div>
    </div>
  );
}

export default App;
