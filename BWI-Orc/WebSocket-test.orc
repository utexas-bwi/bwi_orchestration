import site WebSocketClient = "orc.lib.web.WebSocketClient"

val ws = WebSocketClient("ws://ajf.me:8080/", [])
Println("Sending")  >>
ws.send("Test echo")  >>
Println("Receiving")  >>
ws.receive()  >r>
Println(r)  >>
Println("Closing")  >>
ws.closeSend()  >>
Println("awaitReceiveClosed")  >>
ws.awaitReceiveClosed()  >>
Println("Closed.")
