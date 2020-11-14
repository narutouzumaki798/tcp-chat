var net = require("net")

var server = net.createServer();

var user_names = [];
var phase = [];
var idx = 0;
var sockets = {};
var sock_map = {};


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
function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

server.on("connection", function(socket){
    console.log("node: notun connection hoeche socket: " + socket._handle.fd);
    phase[socket._handle.fd] = 0;

    socket.on("data", async function(buffer){
	fd = socket._handle.fd;
	console.log("socket: " + fd + " data: " + buffer + " phase: " + phase[fd]);
	console.log("user_names: " + user_names);
	console.log("find: " + user_names.indexOf(String(buffer)));
	console.log("buffer[0]: " + typeof(buffer[0]) + " : " + buffer[0]);
	

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
			sockets[user_names[i]].write("     you joined as " + sock_map[fd] + "\n\n");
		    else
			sockets[user_names[i]].write("     " + sock_map[fd] + " joined\n\n");
		}
	    }
	    else
	    {
		socket.write("no");
	    }
	    break;

	    case 1: // phase 1 (data)
	    for(i=0; i<user_names.length; i++)
	    {
		sender = "";
		if(sock_map[fd] == user_names[i]) sender = "you";
		else sender = sock_map[fd];
		sockets[user_names[i]].write(sender + "1: " + buffer + "\n\n");
	    }
	    break;

	}
	console.log("---\n");
    });

    socket.on("end", function(){
	fd = socket._handle.fd;
	for(i=0; i<user_names.length; i++) // sending disconnected message
	{
	    if(sock_map[fd] != user_names[i])
		sockets[user_names[i]].write("     " + sock_map[fd] + " disconnected\n\n");
	}
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




