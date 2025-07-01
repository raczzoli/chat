import ssl
import asyncio
import websockets
import json
import random

SERVER_URL = "wss://localhost:8443"  # ← Cseréld ki a saját szerveredre

RESPONSES = [
	"Szia! 🙂", "Mesélj magadról!", "Mit szeretsz csinálni?",
	"Haha, ez jó volt!", "Mi a kedvenc filmed?", "😀", "Érdekes!"
]

# Globális task lista és bot számláló
tasks = []
bot_id_counter = 0

async def chat_bot(gender: str, looking_for: str, bot_id: int):
	# SSL context létrehozása
	ssl_context = ssl.create_default_context()

	# Ha saját tanúsítványt használsz (nem trusted CA), akkor:
	ssl_context.check_hostname = False
	ssl_context.verify_mode = ssl.CERT_NONE

	async with websockets.connect(SERVER_URL, ssl=ssl_context) as ws:
		print(f"Csatlakozva: {gender} keres {looking_for}")

		# Regisztráció
		await ws.send(json.dumps({
			"command": "register",
			"params": {
				"gender": gender,
				"looking_for": looking_for
			}
		}))

		# Várja az üzeneteket
		while True:
			raw = await ws.recv()
			print("Szervertől:", raw)

def add_bot(gender: str, looking_for: str):
	global bot_id_counter
	bot_task = chat_bot(gender, looking_for, bot_id_counter)
	tasks.append(bot_task)
	bot_id_counter += 1

async def main():
	for _ in range(10):  # 10 ciklus
		add_bot("F", "M")  # nő keres férfit
		add_bot("M", "F")  # férfi keres nőt
		add_bot("F", "F")  # nő keres nőt
		add_bot("M", "M")  # férfi keres férfit

	await asyncio.gather(*tasks)

if __name__ == "__main__":
	# asyncio.run(main())  ← töröld vagy kommentezd ki
	loop = asyncio.get_event_loop()
	loop.run_until_complete(main())
