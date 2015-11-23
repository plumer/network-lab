create socket
listen socket
while true
	accept socket request from client
	create a new thread to handle it




handle:
	// the client has no currently online users
	// so wait for register or login
	while true
		receive a packet from client
		if register
			handle registration
		if login
			handle login act
		if logout
			handle logout act
