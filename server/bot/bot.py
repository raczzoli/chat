import ssl
import asyncio
import websockets
import json
import random

# Uncomment to enable debug logging
# import logging
# logging.basicConfig(level=logging.DEBUG)

SERVER_URL = "wss://localhost:8443"  # ← Replace with your own server

responses_list = [
	{"key": ["szia", "szio", "hali", "hi", "hello", "helo"], "answers": ["Szia", "Szió", "Hali", "Hello"]},
	{"key": ["hogyvagy"], "answers": ["Jól, köszi!", "Minden oké!", "Remekül!"]},
	{"key": ["micsi", "mizujs"], "answers": ["Semmi kulonoset, unalom van :))", "Minden oke", "Unalom", "Most ebredtem", "Masztizok :)"]},
	{"key": ["hanyeves", "eves", "korod"], "answers": ["19", "22", "21", "16", "17"]},
	{"key": ["hogyvagy"], "answers": ["Koszi, minden ok", "Jol", "Turhetoen", "Minden ok, unalom"]},
]

# Global task list and bot ID counter
tasks = []
bot_id_counter = 0


async def handle_chat_message(ws, obj: dict):
    text = obj.get("text", "")
    text = text.lower().strip()  # kisbetű + szóközök eltávolítása

    print("Received chat message:", text)

    response_text = "Szia"  # default válasz

    for entry in responses_list:
        for key in entry["key"]:
            if key in text:
                response_text = random.choice(entry["answers"])
                break
        else:
            continue
        break

    await ws.send(json.dumps({
        "command": "message",
        "params": {
            "text": response_text
        }
    }))

async def handle_server_response(ws, raw: str):
	try:
		data = json.loads(raw)
	except json.JSONDecodeError:
		print("Received non-JSON message:", raw)
		return

	command = data.get("command")
	params = data.get("params", {})

	if command == "register-ok":
		pass
	elif command == "message":
		await handle_chat_message(ws, params);
	else:
		print("Unknown command:", command)

async def chat_bot(gender: str, looking_for: str, bot_id: int):
	# Create SSL context
	ssl_context = ssl.create_default_context()

	# If you use self-signed certificate (not trusted CA), then:
	ssl_context.check_hostname = False
	ssl_context.verify_mode = ssl.CERT_NONE

	async with websockets.connect(SERVER_URL, ssl=ssl_context) as ws:
		print(f"Connected: {gender} looking for {looking_for}")

		# Registration
		await ws.send(json.dumps({
			"command": "register",
			"params": {
				"gender": gender,
				"looking_for": looking_for
			}
		}))

		# Wait for messages
		while True:
			try:
				raw = await ws.recv()
				print("From server:", raw)
				await handle_server_response(ws, raw)
			except websockets.ConnectionClosed as e:
				print(f"Connection closed: {e.code} - {e.reason}")
				break
			except Exception as e:
				print(f"Error: {e}")
				break

def add_bot(gender: str, looking_for: str):
	global bot_id_counter
	bot_task = chat_bot(gender, looking_for, bot_id_counter)
	tasks.append(bot_task)
	bot_id_counter += 1

async def main():
	for _ in range(10):  # 10 iterations
		add_bot("F", "M")  # female looking for male
		add_bot("M", "F")  # male looking for female
		add_bot("F", "F")  # female looking for female
		add_bot("M", "M")  # male looking for male

	await asyncio.gather(*tasks)

if __name__ == "__main__":
	# asyncio.run(main())  ← comment out or remove
	loop = asyncio.get_event_loop()
	loop.run_until_complete(main())