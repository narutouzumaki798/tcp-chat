var net = require("net")
var server = net.createServer();

var user_names = [];
var active_users = [];
var phase = [];
var idx = 0;
var sockets = {};
var pass = {};
var sock_map = {};
var img;
var img_size = 0;
var read_bytes = 0;
var sent_bytes = 0;
var recipients = []

function img_send_some(socket)
{
    u = sock_map[socket._handle.fd];
    for(i = 0; i<recipients.length; i++)
    {
	sender = "";
	if(active_users.indexOf(recipients[i]) == -1) continue;
	if(recipients[i] == u) continue;

	sockets[recipients[i]].write(img);
	sender = u;

	payload = ".....[img] " + sender + "[to " + String(recipients) + "]: test.png\n";
	frame = Buffer.from(payload, 'utf8');
	psize = frame.length - 5;

	frame[0] = 116;
	frame[1] = (psize & 0xFF000000)>>24;
	frame[2] = (psize & 0x00FF0000)>>16;
	frame[3] = (psize & 0x0000FF00)>>8;
	frame[4] = (psize & 0x000000FF);

	sockets[recipients[i]].write(frame);
    }
    // sending sender
    payload = ".....[img] " + "you" + "[to " + String(recipients) + "]: test.png\n";
    frame = Buffer.from(payload, 'utf8');
    psize = frame.length - 5;

    frame[0] = 116;
    frame[1] = (psize & 0xFF000000)>>24;
    frame[2] = (psize & 0x00FF0000)>>16;
    frame[3] = (psize & 0x0000FF00)>>8;
    frame[4] = (psize & 0x000000FF);

    socket.write(frame);
}
function img_send_all(socket)
{
    for(i = 0; i<active_users.length; i++)
    {
	if(sock_map[socket._handle.fd] != active_users[i])
	sockets[active_users[i]].write(img); // nije ke pathate hobe na

	sender = "";
	if(sock_map[socket._handle.fd] == active_users[i]) sender = ">> you";
	else sender = sock_map[socket._handle.fd];

	payload = ".....[img] " + sender + ": test.png\n";
	frame = Buffer.from(payload, 'utf8');
	psize = frame.length - 5;

	frame[0] = 116;
	frame[1] = (psize & 0xFF000000)>>24;
	frame[2] = (psize & 0x00FF0000)>>16;
	frame[3] = (psize & 0x0000FF00)>>8;
	frame[4] = (psize & 0x000000FF);

	sockets[active_users[i]].write(frame);
	console.log("img: sending " + active_users[i]);
    }
}
server.on("connection", function(socket){
    console.log("node: notun connection socket: " + socket._handle.fd);
    phase[socket._handle.fd] = 0;

    socket.on("data", async function(buffer){
	fd = socket._handle.fd;
	
	if(read_bytes < img_size) // currently receiving image
	{
	    img = Buffer.concat([img, buffer]);
	    read_bytes = read_bytes + buffer.length;
	    console.log("img asche: " + read_bytes + " / " + img_size + " bytes");
	    if(read_bytes >= img_size) // finished receiving image
	    {
		console.log("img ese geche: " + read_bytes + " / " + img_size + " bytes  img.size=" + img.length);
		console.log("---\n");

		read_bytes = 0;
		img_size = 0;
		if(recipients.length == 0)
		img_send_all(socket);
		else img_send_some(socket);
	    }
	    return;
	}

	console.log("Phase: "  + phase[fd]);

	// t = 116, i = 105
	if(buffer[0] == 116) // console log for text
	{
	    console.log("socket: " + fd + " data: " + buffer + " phase: " + phase[fd]);
	    console.log("buffer[0]: " + typeof(buffer[0]) + " : " + buffer[0]);
	    console.log("buffer length: " + buffer.length);
	}

	switch(phase[fd])
	{
	    case 0: // phase 0 (login)
	    if(user_names.indexOf(String(buffer)) == -1) // new user name
	    {
		sockets[String(buffer)] = socket;
		sock_map[fd] = String(buffer);
		socket.write("ok");
		phase[fd] = 1;
		// await sleep(1000); // sleep to ensure ok is received separately
	    }
	    else // old user name
	    {
		sockets[String(buffer)] = socket; // update socket of old user
		sock_map[fd] = String(buffer); // map new socket file descriptor to old user
		socket.write("no");
		phase[fd] = 2;
	    }
	    break;

	    case 1: // phase 1 new user
	    pass[sock_map[fd]] = String(buffer);
	    if(pass[sock_map[fd]].length == 0)
	    {
		console.log("invalid password"); break;
	    }

	    socket.write("ok"); // faltu
	    phase[fd] = 3;
	    console.log("new user");
	    user_names.push(sock_map[fd]);
	    // active_users.push(sock_map[fd]);
	    break;

	    case 2: // phase 2 old user
	    if(String(buffer) == pass[sock_map[fd]]) socket.write("ok");
	    else { socket.write("no"); phase[fd] = 0; break; }
	    phase[fd] = 3;
	    console.log("old user");

	    // active_users.push(sock_map[fd]);
	    break;

	    case 3: // users list
	    console.log("phase 3 sending users list");
	    tmp = ""
	    for(i=0; i<active_users.length; i++) tmp = tmp + active_users[i] + "\n"; // get list of active users
	    tmp = tmp + sock_map[fd] + "\n"; // current user
	    socket.write(tmp);
	    phase[fd] = 4;
	    break;

	    case 4: // faltu
	    console.log("phase 4 data:" + String(buffer));
	    active_users.push(sock_map[fd]); // er por onnno client der data jabe
	    for(i=0; i<active_users.length; i++) // sending joined message
	    {
		payload = "";
	        if(sock_map[fd] == active_users[i]) { payload = "// you joined as " + sock_map[fd] + "\n"; }
	        else { payload = "// " + sock_map[fd] + " joined\n"; }
		payload = "....." + payload;
		frame = Buffer.from(payload, 'utf8');
		psize = frame.length - 5;

		if(sock_map[fd] == active_users[i]) frame[0] = 116;
		else frame[0] = 99;
		frame[1] = (psize & 0xFF000000)>>24;
		frame[2] = (psize & 0x00FF0000)>>16;
		frame[3] = (psize & 0x0000FF00)>>8;
		frame[4] = (psize & 0x000000FF);

	    	sockets[active_users[i]].write(frame);
	    }
	    phase[fd] = 5;
	    break;


	    case 5: // phase 5 (data)
	    switch(buffer[0])
	    {
		case 116: // [t] text
		console.log("text eseche");
		buffer = buffer.slice(1);
		for(i=0; i<active_users.length; i++)
		{
		    sender = "";
		    if(sock_map[fd] == active_users[i]) sender = ">> you";
		    else sender = sock_map[fd];

		    payload = "....." + sender + ": " + buffer + "\n";
		    frame = Buffer.from(payload, 'utf8');
		    psize = frame.length - 5;

		    frame[0] = 116;
		    frame[1] = (psize & 0xFF000000)>>24;
		    frame[2] = (psize & 0x00FF0000)>>16;
		    frame[3] = (psize & 0x0000FF00)>>8;
		    frame[4] = (psize & 0x000000FF);

		    console.log("payload: " + payload);
		    console.log(frame);
		    console.log(psize);
		    sockets[active_users[i]].write(frame);
		}
		break;

		case 105: // [i] image
		img_size = (buffer[1] << 24) | (buffer[2] << 16) | (buffer[3] << 8) | (buffer[4]);
		console.log("image size: " + img_size);
		img = buffer.slice(0);
		read_bytes = buffer.length - 5;
		if(read_bytes >= img_size)
		{
		    console.log("img ekbarei ese geche: " + read_bytes + " / " + img_size + " bytes");
		    read_bytes = 0;
		    img_size = 0;
		    img_send_all(socket);
		}
		recipients.length = 0;
		break;

		case 114: // [r] recipient list ache
		console.log("recipent list ache:");
		console.log("delimeter: " + buffer.indexOf(1)); d = buffer.indexOf(1);
		data =  String(buffer.slice(d+1)); console.log("text data: " + buffer.slice(d+1));
		recipients = String(buffer.slice(1, d));
		recipients = recipients.split(" ");

		i = 0
		while(i < recipients.length)
		{
		    // console.log("removing blank: " + typeof(recipients[i]) + "  " + typeof(""));
		    if(recipients[i] == "")
			recipients.splice(i, 1);
		    else i++;
		} // removing blanks

		console.log("recipients: " + recipients);
		for(i=0; i<recipients.length; i++)
		{
		    sender = "";
		    if(active_users.indexOf(recipients[i]) == -1) continue;
		    if(recipients[i] == sock_map[fd]) continue;
		    sender = sock_map[fd];

		    payload = "....." + sender + "[to " + String(recipients) + "]: " + data + "\n";
		    frame = Buffer.from(payload, 'utf8');
		    psize = frame.length - 5;

		    frame[0] = 116;
		    frame[1] = (psize & 0xFF000000)>>24;
		    frame[2] = (psize & 0x00FF0000)>>16;
		    frame[3] = (psize & 0x0000FF00)>>8;
		    frame[4] = (psize & 0x000000FF);

		    sockets[recipients[i]].write(frame);
		}
		// sending sender
		payload = "....." + ">> you" + "[to " + String(recipients) + "]: " + data + "\n";
		frame = Buffer.from(payload, 'utf8');
		psize = frame.length - 5;

		frame[0] = 116;
		frame[1] = (psize & 0xFF000000)>>24;
		frame[2] = (psize & 0x00FF0000)>>16;
		frame[3] = (psize & 0x0000FF00)>>8;
		frame[4] = (psize & 0x000000FF);

		socket.write(frame);
		break;

		case 115: // [s] recipient list ache + image
		console.log("recipent list ar image ache:");
		console.log("delimeter: " + buffer.indexOf(1)); d = buffer.indexOf(1);

		recipients = String(buffer.slice(1, d));
		recipients = recipients.split(" ");
		console.log("recipients: " + recipients);
		i = 0
		while(i < recipients.length)
		{
		    // console.log("removing blank: " + typeof(recipients[i]) + "  " + typeof(""));
		    if(recipients[i] == "")
			recipients.splice(i, 1);
		    else i++;
		} // removing blanks

		img_size = (buffer[d+1] << 24) | (buffer[d+2] << 16) | (buffer[d+3] << 8) | (buffer[d+4]);
		console.log("image size: " + img_size);
		img = buffer.slice(d); img[0] = 105; // prothom byte e i bosate hobe
		console.log("img surute: "); console.log(img.slice(0, 5));
		read_bytes = buffer.length - (d+5);
		if(read_bytes >= img_size)
		{
		    console.log("img ekbarei ese geche: " + read_bytes + " / " + img_size + " bytes");
		    read_bytes = 0;
		    img_size = 0;
		    img_send_some(socket);
		}
		break;

		case 120: // [x] disconnect
		fd = socket._handle.fd;
		if(phase[fd] == 5)
		{
		    for(i=0; i<active_users.length; i++) // sending disconnected message
		    {
			if(sock_map[fd] == active_users[i]) continue;
			payload = ".....// " + sock_map[fd] + " disconnected\n";
			frame = Buffer.from(payload, 'utf8');
			psize = frame.length - 5;

			frame[0] = 100;
			frame[1] = (psize & 0xFF000000)>>24;
			frame[2] = (psize & 0x00FF0000)>>16;
			frame[3] = (psize & 0x0000FF00)>>8;
			frame[4] = (psize & 0x000000FF);

			sockets[active_users[i]].write(frame);
		    }
		    r = active_users.indexOf(sock_map[fd]);
		    if(r != -1) active_users.splice(r, 1);
		}
		phase[fd] = 0;
		console.log("first byte x: disconnect " + sock_map[fd]);
		break;
		
	    } // inner switch
	    break;
	} // outer switch
	console.log("---\n");
    });
    socket.on("end", function(){
	console.log("end called");
	console.log("---\n");
    });
    socket.on("close", function(){
	console.log("close called");
	console.log("---\n");
    });
    socket.on("error", function(){
	console.log("error called");
	console.log("---\n");
    });
});
server.listen(8001, function(){
    console.log("server listening to port: 8001") 
});


// test
// ----------------
// text
// image
// text + recipient x 2
// image + recipient x 2
// connect
// disconnect

