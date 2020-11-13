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

server.on("connection", function(socket){
    console.log("node: connection hoeche socket: " + socket._handle.fd);
    phase[socket._handle.fd] = 0;

    socket.on("data", function(buffer){
	fd = socket._handle.fd;
	console.log("socket: " + fd + " data: " + buffer + " phase: " + phase[fd]);
	console.log("user_names: " + user_names);
	console.log("find: " + user_names.indexOf(String(buffer)));
	

	switch(phase[fd])
	{
	    case 0: // phase 0 (login)
	    if(user_names.indexOf(String(buffer)) == -1)
	    {
		user_names.push(String(buffer));
		sockets[String(buffer)] = socket;
		socket.write("ok");
		sock_map[fd] = String(buffer);
		phase[fd]++;
	    }
	    else
	    {
		socket.write("no");
	    }
	    break;

	    case 1: // phase 1 (data)
	    for(i=0; i<user_names.length; i++)
	    {
		sockets[user_names[i]].write(sock_map[fd] + "1: " + buffer + "\n");
	    }
	    break;
	    
	}
	
    });

    socket.on("close", function(){
	
    });
});

server.listen(8001, function(){
    console.log("server listening to port: 8001") 
});

