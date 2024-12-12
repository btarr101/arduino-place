import { RgbColor } from "react-colorful";

const baseUrl = "https://arduino-place-server-lrpx.shuttle.app";

export async function getLeds(): Promise<RgbColor[]> {
	const response = await fetch(`${baseUrl}/leds`);
	
	if (!response.ok) {
		throw Error(response.statusText);
	}

	const colors = await response.json() as { red: number, green: number, blue: number }[];
	return colors.map(({ red, green, blue}) => ({ r: red, g: green, b: blue }));
}

export async function postLed(index: number, color: RgbColor) {
	const response = await fetch(`${baseUrl}/leds/${index}`, {
		method: "POST",
		headers: {
			"Content-Type": "application/x-www-form-urlencoded"
		},
		body: new URLSearchParams({
			red: color.r.toString(),
			green: color.g.toString(),
			blue: color.b.toString()
		})
	});
	
	if (!response.ok) {
		throw Error(response.statusText);
	}
}