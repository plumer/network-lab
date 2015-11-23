send a socket request to server
	if server does not respond
		terminate

prompt user to take action
case login
	prompt user to enter ID and passwd
	check validity
	send to server
	if server returns success
		change flags (current ID, isonline, etc)
case logout
	check if flags are currently valid
	if there exists an online user
		send logout request to server
		
