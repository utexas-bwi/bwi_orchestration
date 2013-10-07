{- rosbridge-test.orc -- Orc program rosbridge-test
 - 
 - $Id$
 - 
 - Created by jthywiss on Aug 26, 2013 3:45:17 PM
 -}

import site WebSocketClient = "orc.lib.web.WebSocketClient"
--import site DumpState = "orc.lib.DumpState"

include "rosbridge.inc"

val ws = WebSocketClient("ws://hypnotoad.csres.utexas.edu:9090/", [])
val jsonWs = JsonOverWebsocketsConnection(ws)
val rb = RosbridgeConnection(jsonWs)
val move1 = RosActionClient(rb, "/robot1/move_base", "move_base_msgs/MoveBase")  #

--(Prompt("Ready to dump?") >> DumpState()) |
--rb.subscribe("/clock").read()
-- rb.advertise("/rosout", "rosgraph_msgs/Log").publish({. msg = "Blurf" .})
move1({. target_pose = {. header = {. frame_id = "/map" .},
                          pose = {. position = {. x=0.0, y=8.0, z=0.0 .} , 
                                    orientation = {. x=0.0, y=0.0, z=0.0, w=1.0 .} .} .} .})
>goal1>
( goal1.feedback() >f> ("feedback: "+f | "feedback status="+goal1.status())
| goal1.result() >r> ("result: "+r | "result status="+goal1.status() | move1.close() >> rb.close())
| "initial status="+goal1.status()
)
