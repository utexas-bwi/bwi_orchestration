{- Patrol.orc -- Orc program Patrol
 - 
 - $Id$
 - 
 - Created by jthywiss on Aug 16, 2013 3:24:48 PM
 -}

include "rosbridge.inc"

import site WebSocketClient = "orc.lib.web.WebSocketClient"
val ws = WebSocketClient("ws://hypnotoad.csres.utexas.edu:9090/", [])
val jsonWs = JsonOverWebsocketsConnection(ws)
val rb = RosbridgeConnection(jsonWs)

{- A robot.  It can "moveTo" a coordinate and report its "pose". -}
def class Robot(rosbridgeConnection, robotId) =
  val allocatedRobot = 
    rosbridgeConnection.call(robotId+"/invite", {. remote_target_name = "orchestrator", application_namespace = "robot1" .})  >>
    rosbridgeConnection.call(robotId+"/start_app", {. name = "segbot_navigation/map_nav" .} )  >>
    signal
  def getId() = robotId
  val moveBase = allocatedRobot >> RosActionClient(rosbridgeConnection, robotId+"/move_base", "move_base_msgs/MoveBase")
  val lastPose = Ref()
  def moveTo((x,y,θ)) =
     import class Math = "java.lang.Math"
     moveBase({.
      target_pose = {. header = {. frame_id = "/map" .},
                        pose = {. position = {. x=x, y=y, z=0.0 .} , 
                                  orientation = {. w=Math.cos(θ/2.0), x=0.0, y=0.0, z=Math.sin(θ/2.0) .} .} .}
    .})  >moveBaseGoal>
    ( moveBaseGoal.result()
    | moveBaseGoal.feedback() >f> (lastPose := f.base_position.pose) >> stop)
  def pose() = lastPose?
  def poseD() = lastPose.readD()
  stop

{- List of robots we are allowed to use -}
def availableRobots() =
  -- TODO: Discover robots
  [Robot(rb, "/robot1"), Robot(rb, "/robot2"), Robot(rb, "/robot3"), Robot(rb, "/robot4")]

{- Patrol parameters -}
val π = 3.14159265358979323846
val patrolWaypoints = [(0,8,π/2), (0,15,0), (33.5,15,-π/2), (33.5,8,π)]
val numPatrollers = 2

{- Make one pass through the waypoints -}
def patrolOnce(robot, []) = Println("patrolOnce done for "+robot.getId()) >> signal
def patrolOnce(robot, waypoint:waypoints) =
  Println("Move "+robot.getId()+" to "+waypoint) >> robot.moveTo(waypoint) >> Println("Move "+robot.getId()+" done moving to "+waypoint) >> patrolOnce(robot, waypoints)

{- Patrol forever -}
def patrol(robot, waypoints) =
  patrolOnce(robot, waypoints) >> patrol(robot, waypoints)

{- A list rotate utility -}
def rotate(n, xs) = append(drop(n % length(xs), xs), take(n % length(xs), xs))

{- Spread the patrollers out among the waypoints by rotating the waypoint list -}
def spread(robot:robots, step, waypoints) = patrol(robot, waypoints) | spread(robots, step, rotate(step, waypoints))


spread(take(numPatrollers, availableRobots()), 1, patrolWaypoints)
