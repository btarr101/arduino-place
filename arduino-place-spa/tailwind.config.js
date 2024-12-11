import daisyui from "daisyui";
import tailwindClipPath from "tailwind-clip-path";

/** @type {import('tailwindcss').Config} */
export default {
  darkMode: ["class"],
  content: ["./index.html", "./src/**/*.{js,ts,jsx,tsx}"],
  theme: {
    extend: {},
  },
  plugins: [daisyui, tailwindClipPath],
  /** @type {import('daisyui').Config} */
  daisyui: {
    themes: ["business"],
  },
};
