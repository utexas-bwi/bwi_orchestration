{- rosbridge.inc -- Orc include rosbridge
 - 
 - $Id$
 - 
 - Created by jthywiss on Aug 19, 2013 5:12:45 PM
 -}


--------
-- Config parameters
--------

def debug(s :: String) = signal --| Println(s) >> stop


--------
-- JsonOverWebsocketsConnection
--------

type RosbridgeMessage = {. op :: String .}
type WebSocketClient = {.
  send :: lambda[](String) :: Signal,
  receive :: lambda[]() :: String,
  receiveD :: lambda[]() :: String,
  receiveAll :: lambda[]() :: List[String],
  closeSend :: lambda[]() :: Signal,
  abort :: lambda[]() :: Signal,
  isSendClosed :: lambda[]() :: Boolean,
  isReceiveClosed :: lambda[]() :: Boolean,
  awaitReceiveClosed :: lambda[]() :: Signal
.}
type JsonOverWebsocketsConnection = {.
  send :: lambda[](RosbridgeMessage) :: Signal,
  receive :: lambda[]() :: RosbridgeMessage,
  receiveD :: lambda[]() :: RosbridgeMessage,
  receiveAll :: lambda[]() :: List[RosbridgeMessage],
  closeSend :: lambda[]() :: Signal,
  abort :: lambda[]() :: Signal,
  isSendClosed :: lambda[]() :: Boolean,
  isReceiveClosed :: lambda[]() :: Boolean,
  awaitReceiveClosed :: lambda[]() :: Signal
.}

def class JsonOverWebsocketsConnection(connection :: WebSocketClient) =
  -- TODO: Handle fragments and compression
  def send(msg :: RosbridgeMessage) = debug("# RosbridgeMessage.send "+msg) >> connection.send(WriteJSON(msg))
  def receive() = ReadJSON(connection.receive()) >msg> debug("# RosbridgeMessage.receive "+msg) >> msg
  def receiveD() = ReadJSON(connection.receiveD())
  def receiveAll() = map(ReadJSON, connection.receiveAll())
  def closeSend() = connection.closeSend()
  def abort() = connection.abort()
  def isSendClosed() = connection.isSendClosed()
  def isReceiveClosed() = connection.isReceiveClosed()
  def awaitReceiveClosed() = connection.awaitReceiveClosed()
  Iff(connection.isSendClosed()) ; Error("JsonOverWebsocketsConnection must be passed an open WebSocketClient connection")


--------
-- RosbridgeConnection
--------

def class ChannelMap[IdType,ChanMsgType]() =
  import class Hashtable = "java.util.Hashtable"
  val channelStore = Hashtable[IdType, Channel[ChanMsgType]]()

  def class MappedChan(id :: IdType) =
    val ch = Channel[ChanMsgType]()
    def get() = ch.get()
    def getD() = ch.getD()
    def put(item :: ChanMsgType) = ch.put(item)
    def close() = channelStore.remove(id) >> ch.close()
    def closeD() = channelStore.remove(id) >> ch.closeD()
    def isClosed() = ch.isClosed()
    def getAll() = ch.getAll()
    def readOneAndClose() = ch.get() >r> channelStore.remove(id) >> ch.closeD() >> ch.getAll() >> r
    channelStore.put(id, ch) >prev> Ift(prev /= null) >> Error("Multiple MappedChan for id "+id)

  def newChannel(id :: IdType) = MappedChan(id)
  def channelForId(id :: IdType) = channelStore.get(id) >c> Ift(c /= null) >> c
  def allChannels() = (
    import class JavaConversions = "scala.collection.JavaConversions"
    JavaConversions.collectionAsScalaIterable(channelStore.values()).toList() :!: List[Channel[ChanMsgType]]
  )
  stop

type PubSubMsgPayload = {. .}

type RosbridgeConnection = {.
  advertise :: lambda(String, String) :: {.
    publish :: lambda(PubSubMsgPayload) :: Signal,
    unadvertise :: lambda() :: Signal
  .},
  subscribe :: lambda(String) :: {.
    read :: lambda() :: PubSubMsgPayload,
    unsubscribe :: lambda() :: Signal 
  .},
  call :: lambda(String, List[Top]) :: Top,
  close :: lambda() :: Signal,
  abort :: lambda() :: Signal, 
  isClosed :: lambda() :: Boolean
.}

def class RosbridgeConnection(connection :: JsonOverWebsocketsConnection) =
  import class AtomicLong = "java.util.concurrent.atomic.AtomicLong"
  val lastExchangeId = AtomicLong()
  def getNewExchangeId() = lastExchangeId.incrementAndGet().toString()

  val responseChans = ChannelMap[String, {. .}]()

  import class SynchronizedMultiHashMap = "orc.lib.SynchronizedMultiHashMap"
  val subscriptions = SynchronizedMultiHashMap[String, String]()

  def class Pub(topicName :: String, typeName :: String) =
    val publicationId = "pubr-"+getNewExchangeId()
    def publish(msg :: PubSubMsgPayload) = connection.send({. op ="publish", id = "pubn-"+getNewExchangeId(), topic = topicName, msg = msg .})
    def unadvertise() = connection.send({. op ="unadvertise", id = publicationId, topic = topicName .})
    connection.send({. op ="advertise", id = publicationId, topic = topicName.} + ReadJSON("{ \"type\": \""+typeName+"\" }"))

  def class Sub(topicName :: String) =
    val subscriptionId = "subr-"+getNewExchangeId()
    val respChan = responseChans.newChannel(subscriptionId)
    def read() = respChan.get()
    -- needed?:  def readAll() = respChan.getAll()
    def unsubscribe() = 
      connection.send({. op ="unsubscribe", id = subscriptionId, topic = topicName .}) >>
      respChan.closeD() >>
      (subscriptions.removeBinding(topicName, subscriptionId) :!: SynchronizedMultiHashMap[String, String]) >>
      signal

    debug("# Sub "+topicName) >>
    (subscriptions.addBinding(topicName, subscriptionId) :!: SynchronizedMultiHashMap[String, String]) >>
    connection.send({. op ="subscribe", id = subscriptionId, topic = topicName .})

  def advertise(topicName :: String, typeName :: String) = Pub(topicName, typeName)

  def subscribe(topicName :: String) = Sub(topicName)

  def invoke(callId :: String, serviceName :: String, args :: List[Top]) =
    debug("# RosbridgeConnection.invoke "+serviceName+args) >>
    {. op = "call_service", id = callId, service = serviceName .}  >m>
    (if args = [] then m else m + {. args = args .})  >m'>
    connection.send(m')  >>
    callId

  def call(serviceName :: String, args :: List[Top]) =
    val callId = "call-"+getNewExchangeId()
    type ReplyJoinType = {. op :: String, values :: Top, msg :: String .}
    def handleReply(ReplyJoinType) :: Top
    def handleReply({. op = "service_response", values = v .}) = v
    def handleReply({. op = "service_response" .}) = signal
    def handleReply({. op = "status", msg = errMsg .}) = Error(serviceName+" call failed: " + errMsg)
    def handleReply(msg) = Error("RosbridgeConnection.call: Unexpected response to call "+callId +": " +msg)
    debug("# RosbridgeConnection.call "+serviceName+args) >>
    responseChans.newChannel(callId)  >rc>
    invoke(callId, serviceName, args)  >>
    handleReply(rc.readOneAndClose() :!: ReplyJoinType)

  def close() = connection.closeSend() >> connection.awaitReceiveClosed() >> (each(responseChans.allChannels()) >ch> ch.closeD() >> stop ; debug("# RosbridgeConnection closed"))
  def abort() = (connection.abort() | each(responseChans.allChannels()) >ch> ch.closeD()) >> stop ; debug("# RosbridgeConnection aborted")
  def isClosed() = connection.isSendClosed()

  def readLoop() :: Bot =
    type MsgsJoinType = {. op :: String, id :: String, topic :: String, msg :: PubSubMsgPayload .}
    def handleMsg(MsgsJoinType) :: Signal
    def handleMsg({. op = "publish", topic = topicName, msg = msg .}) =
      type Ugh = {. isEmpty :: lambda() :: Boolean, get :: lambda() :: {. toList :: lambda() :: List[String] .} .}
      -- Unclear in rosbridge docs: what is "id" field for in this message?
      (subscriptions.get(topicName) :!: Ugh) >maybeSubs>
      Iff(maybeSubs.isEmpty()) >>
      ( each(maybeSubs.get().toList()) >subId>
        responseChans.channelForId(subId).put(msg)
      ) >> stop ; signal
    def handleMsg({. id = id .} as msg) =
      responseChans.channelForId(id).put(msg)
    def handleMsg(msg) = Error("RosbridgeConnection: Unexpected message received: " +msg)
    connection.receive()  >msg>
    ( (handleMsg(msg :!: MsgsJoinType) >> stop)
    | readLoop()
    ) #
  readLoop() ; debug("# RosbridgeConnection exit")


--------
-- ROS actions
--------

type GoalStatus = Pending() | Active() | Preempted() | Succeeded() | Aborted() | Rejected() | Preempting() | Recalling() | Recalled() | Lost() | WaitingForGoalAck()

def class RosActionClient[GoalType, ResultType, FeedbackType](connection :: RosbridgeConnection, actionName :: String, actionSpecName :: String) =
  import class AtomicLong = "java.util.concurrent.atomic.AtomicLong"
  val lastGoalId = AtomicLong()
  type GoalIdType = String
  def getNewGoalId() :: GoalIdType = "goal-"+lastGoalId.incrementAndGet().toString()

  import class Hashtable = "java.util.Hashtable"
  val goalStatuses = Hashtable[GoalIdType, GoalStatus]()
  val resultChans = ChannelMap[GoalIdType, PubSubMsgPayload]()
  val feedbackChans = ChannelMap[GoalIdType, PubSubMsgPayload]()

  val goalPub = connection.advertise(actionName+"/goal", actionSpecName+"ActionGoal")
  val cancelPub = connection.advertise(actionName+"/cancel", "actionlib_msgs/GoalID")
  --val statusSub = connection.subscribe(actionName+"/status")
  val resultSub = connection.subscribe(actionName+"/result")
  val feedbackSub = connection.subscribe(actionName+"/feedback")

  def apply(goal :: GoalType) =
    val goalId = {. id = getNewGoalId() .}
    val resultChan = resultChans.newChannel(goalId.id)
    val feedbackChan = feedbackChans.newChannel(goalId.id)
    val status = lambda() = goalStatuses.get(goalId.id)
    val feedback = lambda() = repeat(lambda() = feedbackChan.get().feedback :!: FeedbackType)
    val result = lambda() = resultChan.get().result :!: ResultType
    val cancel = lambda() = cancelPub.publish(goalId)
    goalStatuses.put(goalId.id, WaitingForGoalAck())  >>
    goalPub.publish({. goal_id = goalId, goal = goal.})  >>
    {. status = status, feedback = feedback, result = result, cancel = cancel .}
  def cancelAll() = cancelPub.publish({. .})
  def close() =
    cancelAll() >> stop ;
    ( goalPub.unadvertise()
    | cancelPub.unadvertise()
    --| statusSub.unsubscribe()
    | resultSub.unsubscribe()
    | feedbackSub.unsubscribe()
    ) >> stop ; 
    ( each(feedbackChans.allChannels()) >ch> ch.closeD()
    | each(resultChans.allChannels()) >ch> ch.closeD()
    ) >> stop ;
    connection.close()
  def readLoop() :: Bot =
    val goalStatusesByIntVal = [Pending(), Active(), Preempted(), Succeeded(), Aborted(), Rejected(), Preempting(), Recalling(), Recalled(), Lost()]
    type MsgsJoinType = {. status :: {. goal_id :: {. id :: GoalIdType .}, status :: Integer .}, result :: Top, feedback :: Top .}
    def handleMsg(MsgsJoinType) :: Signal
    def handleMsg({. status = {. goal_id = goalId, status = statusCode .}, result = _ .} as msg) =
      goalStatuses.put(goalId.id, index(goalStatusesByIntVal, statusCode))  >>
      resultChans.channelForId(goalId.id).put(msg)  >>
      resultChans.channelForId(goalId.id).closeD()  >>
      feedbackChans.channelForId(goalId.id).closeD()
    def handleMsg({. status = {. goal_id = goalId, status = statusCode .}, feedback = _ .} as msg) =
      goalStatuses.put(goalId.id, index(goalStatusesByIntVal, statusCode))  >>
      feedbackChans.channelForId(goalId.id).put(msg)
    --def handleMsg({. status_list = statusList .}) = ....
    def handleMsg(msg) = Error("RosActionClient: Unexpected message received: " +msg)  #
    (resultSub.read() | feedbackSub.read())  >msg>
    debug("# RosActionClient.handleMsg "+msg) >>
    ( (handleMsg(msg :!: MsgsJoinType) >> stop)
    | readLoop()
    ) #
  (Iff(connection.isClosed()) ; Error("RosActionClient must be passed an open RosbridgeConnection connection") >> stop) >>
  readLoop() ; debug("# RosActionClient exit")
