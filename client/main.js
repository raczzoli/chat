	var socket 	= null;
	const URL 	= "ws://chateljunk.hu:8080";

	var client = {
		gender: "",
		looking_for: ""
	};

	function init()
	{
		const chatInput = document.getElementById('chatInput');
		const chatBox = document.getElementById('chatBox');

		chatInput.addEventListener('keypress', (e) => {
			if (e.key === 'Enter') 
			{
				e.preventDefault();
				const msg = chatInput.value.trim();
				if (msg.length > 0) 
				{
					appendMessage(msg, 'self');
					chatInput.value = '';
					
					// send to client
					sendToServer("message", {text:msg});
				}
			}
		});
	}

	function connectToServer()
	{
		socket = new WebSocket(URL);

		socket.onopen = function() {
			sendToServer("register", client);
		};

		socket.onmessage = function(event) {
			if (!event.data) 
			{
				console.log(event);
				return;
			}

			var data = JSON.parse(event.data);
			if (data)
			{
				switch(data.command)
				{
					case "register-ok":
						nextStep(3, null);
						break;
					case "register-err":
						alert("Sikertelen csatlakozás!");
						document.location = document.location;
						break;
					case "match-found":
						setTimeout(function(){
							nextStep(4, null);
						}, 1000);
						break;
					case "message":
						if (data.params && data.params.text)
						{
							appendMessage(data.params.text, 'partner');
						}
						break;
				}
			}
			//console.log("Message from server:", event.data);
		};

		socket.onerror = function(error) {
			console.error("WebSocket error:", error);
		};

		socket.onclose = function() {
			status.textContent = "Connection closed.";
		};
	}

	function sendToServer(command, params)
	{
		// Create a JSON object to send
		var payload = {command: command, params: params};
		socket.send(JSON.stringify(payload));
	}

	function nextStep(current, value) 
	{
		switch(current)
		{
			case 1:
				client.gender = value;
				break;
			case 2:
				client.looking_for = value;

				setTimeout(function(){
					connectToServer();
				}, 1000);

				break;
		}

		document.getElementById('step' + current).style.display = 'none';
		const next = document.getElementById('step' + (current + 1));
		console.log('step' + (current + 1));
		next.style.display = 'flex';
	}

	function appendMessage(text, sender) 
	{
		if (!text || text.length < 1)
		{
			return;
		}

		const div = document.createElement('div');
		div.classList.add('message', sender);
		div.textContent = (sender === 'self' ? 'Te: ' : 'Partner: ') + text;
		chatBox.appendChild(div);
		chatBox.scrollTop = chatBox.scrollHeight;
	}

