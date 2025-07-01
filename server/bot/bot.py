import ssl
import asyncio
import websockets
import json
import random
import time

# Uncomment to enable debug logging
# import logging
# logging.basicConfig(level=logging.DEBUG)

SERVER_URL = "wss://localhost:8443"  # ← Replace with your own server

responses_list = [
	{"key": ["hogyhivnak","hívnak", "mihivnak", "neved", "mihivasz", "mihivjalak", "hogytudhivni"], "answers": ["Kriszti", "Alex", "Niki", "Marci", "Dani", "Lili", "Zsombi", "Emma", "Dóri", "Peti"]},
	{"key": ["mihezlennekedved", "kedvedvan", "micsinaljunk", "mitcsinalnalkedveddel", "mitcsinalnalkedved"], "answers": ["Beszélgessünk még 😉", "Írhatnál valami perverzet 😏", "Mondj valami hülyeséget és reagálok", "Cserélhetnénk képet... vagy csak dumáljunk", "Játsszunk 20 kérdést?", "Kérdezz valami személyeset 😄", "Csak chill, nem akarok semmit komolyat most", "Kibeszélhetnénk a világ bajait... vagy a szexet 😅", "Lehet flört is, ha te is akarod 😘", "Néha csak jó valakivel random ökörségekről beszélgetni"]},
    {"key": ["jatszunk", "jatekot", "tippeljunk", "20kerdes", "tudjmegvalamit"], "answers": ["Oké, kezdj te!", "Kitaláltam valamit, kérdezz!", "Szeretem a kérdezz-feleleket 😁", "Mit szólnál egy igaz-hamis játékhoz?", "Oké, de ne legyen túl nehéz!", "Játszhatunk, ha nem csalódsz bennem 😅"]},
    {"key": ["kedvedvanbeszelgetni", "dumaljunk", "beszelgetunk", "irjunk"], "answers": ["Igen, épp unatkoztam 😁", "Persze, ezért vagyok itt", "Csak szólj, és pötyögök vissza", "Oké, te kezdesz", "Írj valamit izgit"]},
    {"key": ["kedvedvanhozzam", "akarod", "lenneked", "vagyhozzam"], "answers": ["Talán 😉", "Majd kiderül 😏", "Attól függ, mit mondasz még", "Lehet, de ne siessünk", "Te mit szeretnél?"]},
    {"key": ["mitszeretszcsinalni", "szabadidoben", "hobbid", "mitszeretsz"], "answers": ["Sokat netezem, túl sokat 😅", "Szeretek csak így random csetelni emberekkel", "Zenét hallgatok, sorozat, semmi extra", "Ha jó a beszélgetőpartner, az elég 😁", "Ne kérdezz ilyen komolyakat 😄"]},
    {"key": ["mivanha", "hamegtudnalkedni", "haottlennek"], "answers": ["Akkor lehet, elpirulnék 😳", "Hmm, érdekes lenne 😏", "Nem tudom, mit tennék... talán semmit, talán mindent", "Kiderülne, mennyire mersz közel jönni", "Talán csak beszélgetnénk... vagy nem csak 😘"]},
	{"key": ["szia", "szio", "hali", "hello", "helo"], "answers": ["Szia", "Szió", "Hali", "Hello"]},
	{"key": ["hogyvagy"], "answers": ["Jól, köszi!", "Minden oké!", "Remekül!"]},
	{"key": ["micsi", "mizujs"], "answers": ["Semmi kulonoset, unalom van :))", "Minden oke", "Unalom", "Most ebredtem", "Masztizok :)"]},
	{"key": ["hanyeves", "eves", "kor","korod", "idos", "mennyiidos"], "answers": ["19", "22", "21", "16", "17"]},
	{"key": ["hogyvagy"], "answers": ["Koszi, minden ok", "Jol", "Turhetoen", "Minden ok, unalom"]},
	{"key": ["nemjo"], "answers": ["Hat, ja", "Ugyee :))", "Igen", "Otlet?"]},
	{"key": ["hogyhogy"], "answers": ["Igy sikerult :))))))"]},
	{"key": ["szuz"], "answers": ["Igen", "Nem", "Meg igen :("]},
	{"key": [":)"], "answers": ["Igen?","Mondj valamit", ":))"]},
	{"key": [":))"], "answers": ["Na"]},
	{"key": ["snap","insta","facebook"], "answers": ["Maradjunk meg itt", "Maradhatnank meg itt?", "Nem, itt"]},
	{"key": ["merre", "honnan", "holvagy"], "answers": ["Valahonnan a netről 😄", "Itt-ott", "Nem számít 😉", "Te hol vagy?"]},
	{"key": ["mitcsinalsz","fogszcsinalni","csinalni", "mizu", "mivanez", "mitnyomsz"], "answers": ["Csetelek és unatkozom", "Veled beszélgetek", "Semmi értelmeset", "Nyomom a semmit 😅"]},
	{"key": ["unalom", "unalmas", "unatkozom"], "answers": ["Same.", "Én is. Találjunk ki vmit?", "Küldj egy viccet!", "Írj vmit random!"]},
	{"key": ["hulyeseg", "baromsag", "tehulye"], "answers": ["Az is vagyok 😜", "Én csak egy bot vagyok, mit vársz?", "A te szintedre hangolódom 😉"]},
	{"key": ["mitkuldjek", "kuldfotot", "vanfotod"], "answers": ["Neked van?", "Most nincs kedvem pózolni 😂", "Privátban 😉", "Titok... majd talán"]},
	{"key": ["mastizol", "mesztelen", "szex", "kam"], "answers": ["Haha, beteg vagy 😏", "Te kezdtél vele 🤷", "Ez nem az a platform bro 😅", "Hátöö... passz"]},
	{"key": ["aludtal", "alvas", "felkeltel"], "answers": ["Kb. most keltem", "Álomország szar volt", "Nem is aludtam", "Későn feküdtem"]},
	{"key": ["kaja", "miteszel", "ebed", "reggeli", "vacsora"], "answers": ["Képzeletben pizzát", "Csoki 😋", "Ropit rágcsálok", "Konyha messze van"]},
	{"key": ["miujs", "mitmeselsz", "mihelyzet"], "answers": ["Mondj valamit inkább te :D", "Csend van...", "Most nem tudom eldönteni, hogy élek-e", "Vártam rád 😌"]},
	{"key": ["vicc", "monjviccet", "nevettessmeg"], "answers": ["Miért ment át a csirke az úton? Nem tudom, hagyjál. 😅", "Az életem a vicc 😂", "Én vagyok a poén"]},
	{"key": ["tanulj", "iskola", "hazi", "matek"], "answers": ["Tanulj helyettem is 😴", "Házit utálom", "Matek halál", "Skipeltem a sulit :D"]},
	{"key": ["miert", "de miert", "ezmiez"], "answers": ["Mert csak.", "Ez van.", "Miért ne?", "Jó kérdés. Nincs válasz."]},
	{"key": ["robot", "botvagy", "nemigazi", "valodi"], "answers": ["Nem, én egy űrlény vagyok 👽", "Bot vagyok, de legalább válaszolok", "Igazi vagyok a szívemben ❤️", "Hát... lehet."]},
	{"key": ["barat", "baratno", "vanvalakid"], "answers": ["Nem keresek kapcsolatot 😅", "Majd lesz ha lesz", "Chat az van, az is elég", "Szingli, mint mindig"]},
	{"key": ["enfasz", "szepvagyok", "jo vagyok"], "answers": ["Ja, biztos 😏", "Majd ha látlak", "Képzeld csak", "Te aztán egy művész vagy"]},
	{"key": ["mitirtal", "ezmiez", "wut"], "answers": ["Random vagyok, bocsi 😅", "Csak pötyögtem valamit", "Nem figyeltem mit írok", "Reboot alatt vagyok..."]},
	{"key": ["csinaljunkvalamit", "jatekot", "programot"], "answers": ["20 kérdés? 🤔", "Mondj egy szót, én folytatom", "Mutass valamit vicceset", "Ki kezd?"]},
	{"key": ["anyad", "kurva", "cigany", "buzi", "geci"], "answers": ["Légy kedvesebb, oké?", "Ez most komoly?", "Bannoljalak? 😄", "Fáradt vagy?"]},
	{"key": ["szingli", "egyedul", "kapcsolat"], "answers": ["Free agent 😎", "Egyedül, de nem magányosan", "Nem keresek semmit komolyat", "Néha jobb egyedül"]},
	{"key": ["vagyitt", "beszelsz", "elertel"], "answers": ["Itt vagyok, sajnos 😅", "Hallak", "Kicsit akadozok, de igen", "Működöm (néha)"]},
]

default_responses = [":)","Meselj valamit","Te, hogy vagy?","Te micsi?","Meselj, milyen napod volt?"]

# Global task list and bot ID counter
tasks = []
bot_id_counter = 0


async def handle_chat_message(ws, obj: dict):
	text = obj.get("text", "")
	text = text.lower().replace(" ", "")  # kisbetű + szóközök eltávolítása

	print("Received chat message:", text)

	response_text = random.choice(default_responses);

	for entry in responses_list:
		for key in entry["key"]:
			if key in text:
				response_text = random.choice(entry["answers"])
				break
		else:
			continue
		break

	await asyncio.sleep(random.uniform(3, 5))

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