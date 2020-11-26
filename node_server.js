var net = require("net")

var server = net.createServer();

var user_names = [];
var phase = [];
var idx = 0;
var sockets = {};
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
	if(user_names.indexOf(recipients[i]) == -1) continue;
	if(recipients[i] == u) continue;
	sockets[recipients[i]].write(img);
	sockets[recipients[i]].write("t[img] " + u + "[to " + String(recipients) + "]: test.png\n");
    }
    socket.write("t[img] " + "you" + "[to " + String(recipients) + "]: test.png\n");
}
function img_send_all(socket)
{
    for(i = 0; i<user_names.length; i++)
    {
	if(sock_map[socket._handle.fd] != user_names[i])
	sockets[user_names[i]].write(img); // nije ke pathate hobe na

	sender = "";
	if(sock_map[socket._handle.fd] == user_names[i])
	    sender = ">> you";
	else sender = sock_map[socket._handle.fd];

	// sockets[user_names[i]].once("drain", function(){
	//     sockets[user_names[i]].write("[img] " + sender + ": test.png\n\n");
	// });
	sockets[user_names[i]].write("t[img] " + sender + ": test.png\n\n");
	console.log("img: sending " + user_names[i]);
    }
}
function sleep(ms)
{
  return new Promise(resolve => setTimeout(resolve, ms));
}
function setCharAt(str,index,chr) {
    if(index > str.length-1) return str;
    return str.substring(0,index) + chr + str.substring(index+1);
}
function exists(a)
{
    console.log("exists:");
    console.log("buffer: " + typeof(a));
    for(i=0; i<user_names.length; i++)
    {
    	process.stdout.write(" array" + i + ": " + user_names[i] + " : " + typeof(user_names[i]));
    }
    console.log("\n---\n");
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

		// console.log(img);
		//img = img.toString('hex');
		//img = setCharAt(img, 1, 'i');
		//img = img.slice(1); // reformat to hex string with i at beginning
		//console.log(img.slice(0, 10));

		read_bytes = 0;
		img_size = 0;
		if(recipients.length == 0)
		img_send_all(socket);
		else img_send_some(socket);
	    }
	    return;
	}

	// t = 116, i = 105
	if(buffer[0] == 116) // console log for text
	{
	    console.log("socket: " + fd + " data: " + buffer + " phase: " + phase[fd]);
	    // console.log("user_names: " + user_names);
	    // console.log("find: " + user_names.indexOf(String(buffer)));
	    console.log("buffer[0]: " + typeof(buffer[0]) + " : " + buffer[0]);
	    console.log("buffer length: " + buffer.length);
	}

	switch(phase[fd])
	{
	    case 0: // phase 0 (login)
	    if(user_names.indexOf(String(buffer)) == -1 && String(buffer) != "you")
	    {
		user_names.push(String(buffer));
		sockets[String(buffer)] = socket;
		socket.write("ok");
		sock_map[fd] = String(buffer);
		phase[fd]++;
		await sleep(1000); // sleep to ensure ok is received separately

		for(i=0; i<user_names.length; i++) // sending joined message
		{
		    if(sock_map[fd] == user_names[i])
			sockets[user_names[i]].write("t// you joined as " + sock_map[fd] + "\n");
		    else
			sockets[user_names[i]].write("t// " + sock_map[fd] + " joined\n");
		}
	    }
	    else
	    {
		socket.write("no");
	    }
	    break;

	    case 1: // phase 1 (data)
	    switch(buffer[0])
	    {
		case 116: // text
		console.log("text eseche");
		buffer = buffer.slice(1);
		for(i=0; i<user_names.length; i++)
		{
		    sender = "t";
		    if(sock_map[fd] == user_names[i]) sender = sender + ">> you";
		    else sender = sender + sock_map[fd];
		    sockets[user_names[i]].write(sender + ": " + buffer + "\n");
		}
		break;

		case 105: // image
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

		case 114: // recipient list ache
		console.log("recipent list ache:");
		console.log("delimeter: " + buffer.indexOf(1)); d = buffer.indexOf(1);
		data =  String(buffer.slice(d+1)); console.log("text data: " + buffer.slice(d+1));
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

		console.log("recipients: " + recipients);
		for(i=0; i<recipients.length; i++)
		{
		    if(user_names.indexOf(recipients[i]) == -1) continue;
		    if(recipients[i] == sock_map[fd]) continue;
		    sockets[recipients[i]].write("t" + sock_map[fd] + "[to " + String(recipients) + "]: " + data + "\n");
		}
		sockets[sock_map[fd]].write("t>> you" + "[to " + String(recipients) +"]: " + data + "\n");
		break;

		case 115: // recipient list ache + image
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
		
	    } // inner switch
	    break;
	} // outer switch
	console.log("---\n");
    });

    socket.on("end", function(){
	fd = socket._handle.fd;
	for(i=0; i<user_names.length; i++) // sending disconnected message
	{
	    if(sock_map[fd] != user_names[i])
		sockets[user_names[i]].write("t// " + sock_map[fd] + " disconnected\n");
	}
	r = user_names.indexOf(sock_map[fd]);
	if(r != -1) user_names.splice(r, 1);
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




