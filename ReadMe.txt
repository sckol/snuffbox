
Remote control key bindings:

	See Controls.txt

Command line arguments:

	Parameter name and arguments			Description
	--------------------------------------------------------------------------------------------------------
	-stream_url	<rtsp_server_ip>			IP address of RTSP server
	-rtsp_port <rtsp_server_port>			Port number of RTSP server
	-stream_info_url <http_server_ip>		IP address of HTTP server with stream list (streams.txt file)

	RTSP stream list is downloaded from HTTP server using URL http://<http_server_ip>/streams.txt where
	<http_server_ip> is HTTP server's IP address supplied with -stream_info_url command line parameter and
	streams.txt is a plain text file which contains one file name per line. If connection to HTTP server
	fails or streams.txt file is not found then stream list is loaded from local file
	/opt/elecard/share/StbMainApp/streams.txt.