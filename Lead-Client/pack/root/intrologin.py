import dbg
import app
import net
import ui
import ime
import snd
import wndMgr
import musicInfo
import serverInfo
import systemSetting
import ServerStateChecker
import localeInfo
import constInfo
import uiCommon
import time
import serverCommandParser
import ime
import uiScriptLocale
import uiTarget
import math

LOGIN_DELAY_SEC = 0.0
SKIP_LOGIN_PHASE = False
SKIP_LOGIN_PHASE_SUPPORT_CHANNEL = False
FULL_BACK_IMAGE = False

PASSPOD_MSG_DICT = {}

def IsFullBackImage():
	global FULL_BACK_IMAGE
	return FULL_BACK_IMAGE

def IsLoginDelay():
	global LOGIN_DELAY_SEC
	if LOGIN_DELAY_SEC > 0.0:
		return True
	else:
		return False

def GetLoginDelay():
	global LOGIN_DELAY_SEC
	return LOGIN_DELAY_SEC

app.SetGuildMarkPath("test")

class ConnectingDialog(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__LoadDialog()
		self.eventTimeOver = lambda *arg: None
		self.eventExit = lambda *arg: None

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadDialog(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/ConnectingDialog.py")

			self.board = self.GetChild("board")
			self.message = self.GetChild("message")
			self.countdownMessage = self.GetChild("countdown_message")

		except:
			import exception
			exception.Abort("ConnectingDialog.LoadDialog.BindObject")

	def Open(self, waitTime):
		curTime = time.clock()
		self.endTime = curTime + waitTime

		self.Lock()
		self.SetCenterPosition()
		self.SetTop()
		self.Show()		

	def Close(self):
		self.Unlock()
		self.Hide()

	def Destroy(self):
		self.Hide()
		self.ClearDictionary()

	def SetText(self, text):
		self.message.SetText(text)

	def SetCountDownMessage(self, waitTime):
		self.countdownMessage.SetText("%.0f%s" % (waitTime, localeInfo.SECOND))

	def SAFE_SetTimeOverEvent(self, event):
		self.eventTimeOver = ui.__mem_func__(event)

	def SAFE_SetExitEvent(self, event):
		self.eventExit = ui.__mem_func__(event)

	def OnUpdate(self):
		lastTime = max(0, self.endTime - time.clock())
		if 0 == lastTime:
			self.Close()
			self.eventTimeOver()
		else:
			self.SetCountDownMessage(self.endTime - time.clock())

	def OnPressExitKey(self):
		#self.eventExit()
		return True

PORTAL_PATH = "locale/common/ui/login/portal/"
PORTAL_ORBIT_RADIUS = 234
PORTAL_FLAG_DIM = 0.55
PORTAL_ROTATE_TIME = 0.55

def PortalFont(size):
	return "%s:%d" % (localeInfo.UI_DEF_FONT.split(":")[0], size)

class PortalImage(ui.ExpandedImageBox):

	def __init__(self):
		ui.ExpandedImageBox.__init__(self)
		self.overInEvent = None
		self.overOutEvent = None
		self.imageScale = 1.0
		self.index = 0

	def __del__(self):
		ui.ExpandedImageBox.__del__(self)

	def SetImage(self, fileName):
		self.LoadImage(PORTAL_PATH + fileName)
		self.SetScale(self.imageScale, self.imageScale)

	def OnMouseOverIn(self):
		if self.overInEvent:
			self.overInEvent(self.index)

	def OnMouseOverOut(self):
		if self.overOutEvent:
			self.overOutEvent(self.index)

class LoginWindow(ui.ScriptWindow):

	def __init__(self, stream):
		ui.ScriptWindow.__init__(self)
		net.SetPhaseWindow(net.PHASE_WINDOW_LOGIN, self)
		net.SetAccountConnectorHandler(self)

		self.stream = stream
		self.connectingDialog = None
		self.isNowCountDown = False
		self.idEditLine = None
		self.pwdEditLine = None
		self.loginFailureMsgDict = {}
		self.loginFailureFuncDict = {}
		self.objectList = []
		self.flagList = []
		self.serverItemDict = {}
		self.channelItemDict = {}
		self.selectedServerID = 0
		self.selectedChannelID = 0
		self.focusState = -1
		self.loginButton = None
		self.unit = 1.0
		self.rotateIndex = -1
		self.rotateTarget = 0.0
		self.rotateStart = 0.0

	def __del__(self):
		net.ClearPhaseWindow(net.PHASE_WINDOW_LOGIN, self)
		net.SetAccountConnectorHandler(0)
		ui.ScriptWindow.__del__(self)

	def Open(self):
		ServerStateChecker.Create(self)

		self.loginFailureMsgDict = {
			"ALREADY"	: localeInfo.LOGIN_FAILURE_ALREAY,
			"NOID"		: localeInfo.LOGIN_FAILURE_NOT_EXIST_ID,
			"WRONGPWD"	: localeInfo.LOGIN_FAILURE_WRONG_PASSWORD,
			"FULL"		: localeInfo.LOGIN_FAILURE_TOO_MANY_USER,
			"SHUTDOWN"	: localeInfo.LOGIN_FAILURE_SHUTDOWN,
			"REPAIR"	: localeInfo.LOGIN_FAILURE_REPAIR_ID,
			"BLOCK"		: localeInfo.LOGIN_FAILURE_BLOCK_ID,
			"BESAMEKEY"	: localeInfo.LOGIN_FAILURE_BE_SAME_KEY,
			"NOTAVAIL"	: localeInfo.LOGIN_FAILURE_NOT_AVAIL,
			"NOBILL"	: localeInfo.LOGIN_FAILURE_NOBILL,
			"BLKLOGIN"	: localeInfo.LOGIN_FAILURE_BLOCK_LOGIN,
			"WEBBLK"	: localeInfo.LOGIN_FAILURE_WEB_BLOCK,
			"BADSCLID"	: localeInfo.LOGIN_FAILURE_WRONG_SOCIALID,
			"AGELIMIT"	: localeInfo.LOGIN_FAILURE_SHUTDOWN_TIME,
		}

		self.loginFailureFuncDict = {
			"WRONGPWD"	: self.__DisconnectAndInputPassword,
			"QUIT"		: app.Exit,
		}

		self.SetSize(wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())
		self.SetWindowName("LoginWindow")

		self.__BuildWindow()
		self.__LoadLoginInfo("loginInfo.xml")

		if musicInfo.loginMusic != "":
			snd.SetMusicVolume(systemSetting.GetMusicVolume())
			snd.FadeInMusic("BGM/" + musicInfo.loginMusic)

		snd.SetSoundVolume(systemSetting.GetSoundVolume())

		ime.AddExceptKey(91)
		ime.AddExceptKey(93)

		self.Show()
		self.idEditLine.SetText(getattr(self.stream, "loginAccountText", ""))
		self.idEditLine.SetFocus()
		app.ShowCursor()

	def Close(self):
		if self.connectingDialog:
			self.connectingDialog.Close()
		self.connectingDialog = None

		ServerStateChecker.Initialize(self)

		if musicInfo.loginMusic != "" and musicInfo.selectMusic != "":
			snd.FadeOutMusic("BGM/" + musicInfo.loginMusic)

		if self.idEditLine:
			self.idEditLine.SetTabEvent(0)
			self.idEditLine.SetReturnEvent(0)
		if self.pwdEditLine:
			self.pwdEditLine.SetReturnEvent(0)
			self.pwdEditLine.SetTabEvent(0)

		for flag in self.flagList:
			flag.overInEvent = None
			flag.overOutEvent = None
		if self.loginButton:
			self.loginButton.overInEvent = None
			self.loginButton.overOutEvent = None

		self.idEditLine = None
		self.pwdEditLine = None
		self.userField = None
		self.lockField = None
		self.loginButton = None
		self.languageName = None
		self.serverMark = None
		self.channelRing = None
		self.flagList = []
		self.serverItemDict = {}
		self.channelItemDict = {}
		self.objectList = []

		self.KillFocus()
		self.Hide()

		self.stream.popupWindow.Close()
		self.loginFailureFuncDict = None

		ime.ClearExceptKey()
		app.HideCursor()

	def __X(self, dx):
		return self.centerX + int(round(dx * self.unit))

	def __Y(self, dy):
		return self.centerY + int(round(dy * self.unit))

	def __MakeImage(self, fileName, dx, dy, pick = False):
		image = PortalImage()
		image.SetParent(self)
		image.imageScale = self.unit / 2.0
		image.SetImage(fileName)
		image.SetPosition(self.__X(dx), self.__Y(dy))
		if not pick:
			image.AddFlag("not_pick")
		image.Show()
		self.objectList.append(image)
		return image

	def __MakeText(self, parent, x, y, text, color, fontSize = 12):
		textLine = ui.TextLine()
		textLine.SetParent(parent)
		textLine.SetPosition(x, y)
		textLine.SetFontName(PortalFont(int(round(fontSize * self.unit))))
		textLine.SetPackedFontColor(color)
		textLine.SetText(text)
		textLine.AddFlag("not_pick")
		textLine.Show()
		self.objectList.append(textLine)
		return textLine

	def __MakeEditLine(self, dx, dy, width, secret):
		editLine = ui.EditLine()
		editLine.SetParent(self)
		editLine.SetPosition(self.__X(dx), self.__Y(dy))
		editLine.SetSize(int(width * self.unit), int(22 * self.unit))
		editLine.SetFontName(PortalFont(int(round(15 * self.unit))))
		editLine.SetMax(16)
		editLine.SetIMEFlag(0)
		if secret:
			editLine.SetSecret()
		editLine.SetPackedFontColor(0xfff5f2eb)
		editLine.Show()
		return editLine

	def __BuildWindow(self):
		screenWidth = wndMgr.GetScreenWidth()
		screenHeight = wndMgr.GetScreenHeight()
		self.unit = max(0.8, min(float(screenHeight) / 720.0, float(screenWidth) / 1180.0))
		self.centerX = screenWidth / 2
		self.centerY = screenHeight / 2 + int(12 * self.unit)

		background = ui.ExpandedImageBox()
		background.SetParent(self)
		background.LoadImage(PORTAL_PATH + "background.sub")
		background.SetScale(float(screenWidth) / 2048.0, float(screenHeight) / 1536.0)
		background.SetPosition(0, 0)
		background.AddFlag("not_pick")
		background.Show()
		self.objectList.append(background)

		self.__MakeImage("ring.tga", -280, -280)

		self.userField = self.__MakeImage("field_user_1.tga", -143, -28 - 28)
		self.lockField = self.__MakeImage("field_lock_0.tga", -143, 26 - 28)

		self.idEditLine = self.__MakeEditLine(-89, -28 - 10, 200, False)
		self.pwdEditLine = self.__MakeEditLine(-89, 26 - 10, 168, True)

		self.loginButton = self.__MakeImage("arrow_0.tga", 135 - 20 - 18, 26 - 18, True)
		self.loginButton.overInEvent = ui.__mem_func__(self.__OnOverInLoginButton)
		self.loginButton.overOutEvent = ui.__mem_func__(self.__OnOverOutLoginButton)
		self.loginButton.SetMouseLeftButtonDownEvent(ui.__mem_func__(self.__OnClickLoginButton))

		self.idEditLine.SetReturnEvent(ui.__mem_func__(self.pwdEditLine.SetFocus))
		self.idEditLine.SetTabEvent(ui.__mem_func__(self.pwdEditLine.SetFocus))
		self.pwdEditLine.SetReturnEvent(ui.__mem_func__(self.__OnClickLoginButton))
		self.pwdEditLine.SetTabEvent(ui.__mem_func__(self.idEditLine.SetFocus))

		self.__BuildLanguageRing()
		self.__BuildServerBlock()

	def __OnOverInLoginButton(self, index):
		self.loginButton.SetImage("arrow_1.tga")

	def __OnOverOutLoginButton(self, index):
		self.loginButton.SetImage("arrow_0.tga")

	def __BuildLanguageRing(self):
		self.languageList = localeInfo.GetLanguageList()
		currentIndex = 0
		for index, (code, name) in enumerate(self.languageList):
			if code == app.GetLocaleName():
				currentIndex = index

		self.currentLanguageIndex = currentIndex
		self.__MakeImage("flag_ring.tga", -24, -PORTAL_ORBIT_RADIUS - 24)

		for index, (code, name) in enumerate(self.languageList):
			flag = self.__MakeImage("flag_%s.tga" % code, 0, 0, True)
			flag.index = index
			flag.overInEvent = ui.__mem_func__(self.__OnOverInLanguage)
			flag.overOutEvent = ui.__mem_func__(self.__OnOverOutLanguage)
			flag.SetMouseLeftButtonDownEvent(ui.__mem_func__(self.__OnSelectLanguage), index)
			if index != currentIndex:
				flag.SetAlpha(PORTAL_FLAG_DIM)
			self.flagList.append(flag)

		self.__PlaceFlags(0.0, 0.0)

		self.languageName = self.__MakeText(self, self.__X(0), self.__Y(108), "", 0xffc3ccd6, 14)
		self.languageName.SetHorizontalAlignCenter()

	def __OnOverInLanguage(self, index):
		self.flagList[index].SetAlpha(1.0)
		self.languageName.SetText(self.languageList[index][1])

	def __OnOverOutLanguage(self, index):
		if index != self.currentLanguageIndex:
			self.flagList[index].SetAlpha(PORTAL_FLAG_DIM)
		self.languageName.SetText("")

	def __OnSelectLanguage(self, index):
		if self.rotateIndex >= 0 or index == self.currentLanguageIndex:
			return

		count = max(1, len(self.languageList))
		target = -(index - self.currentLanguageIndex) * 2.0 * math.pi / count
		while target > math.pi:
			target -= 2.0 * math.pi
		while target < -math.pi:
			target += 2.0 * math.pi

		self.rotateIndex = index
		self.rotateTarget = target
		self.rotateStart = app.GetTime()

	def __PlaceFlags(self, offset, progress):
		count = max(1, len(self.languageList))
		for index, flag in enumerate(self.flagList):
			angle = -math.pi / 2.0 + (index - self.currentLanguageIndex) * 2.0 * math.pi / count + offset
			flag.SetPosition(self.__X(PORTAL_ORBIT_RADIUS * math.cos(angle) - 16), self.__Y(PORTAL_ORBIT_RADIUS * math.sin(angle) - 16))
			if index == self.rotateIndex:
				flag.SetAlpha(PORTAL_FLAG_DIM + (1.0 - PORTAL_FLAG_DIM) * progress)
			elif index == self.currentLanguageIndex and self.rotateIndex >= 0:
				flag.SetAlpha(1.0 - (1.0 - PORTAL_FLAG_DIM) * progress)

	def __UpdateRotation(self):
		progress = min(1.0, (app.GetTime() - self.rotateStart) / PORTAL_ROTATE_TIME)
		eased = progress * progress * (3.0 - 2.0 * progress)
		self.__PlaceFlags(self.rotateTarget * eased, eased)

		if progress < 1.0:
			return

		code, name = self.languageList[self.rotateIndex]
		self.rotateIndex = -1
		self.stream.loginAccountText = self.idEditLine.GetText()
		if localeInfo.SetLanguage(code):
			self.stream.SetLoginPhase()

	def __GetServerDict(self):
		return serverInfo.REGION_DICT.get(0, {})

	def __BuildServerBlock(self):
		dx = min(PORTAL_ORBIT_RADIUS + 70, wndMgr.GetScreenWidth() / 2.0 / self.unit - 200)
		dy = -70
		self.blockDX = dx

		serverDict = self.__GetServerDict()
		serverIDList = serverDict.keys()
		serverIDList.sort()

		loadServerID, loadChannelID = self.__LoadChannelInfo()
		if not serverDict.has_key(loadServerID):
			loadServerID = serverIDList[0] if serverIDList else 0

		self.__MakeImage("shade.tga", dx - 70, dy - 70)

		if len(serverIDList) > 1:
			self.__MakeText(self, self.__X(dx), self.__Y(dy), uiScriptLocale.LOGIN_SELECT_TITLE, 0xffc3ccd6, 11)
			dy += 24
			self.serverMark = self.__MakeImage("mark.tga", dx - 20, dy - 6)
			for serverID in serverIDList:
				item = ui.Window()
				item.SetParent(self)
				item.SetPosition(self.__X(dx), self.__Y(dy))
				item.SetSize(int(170 * self.unit), int(28 * self.unit))
				item.SetMouseLeftButtonDownEvent(ui.__mem_func__(self.__SelectServer), serverID)
				item.Show()
				item.textLine = self.__MakeText(item, 0, int(2 * self.unit), serverDict[serverID].get("name", ""), 0xffc3ccd6, 16)
				item.posDY = dy
				self.objectList.append(item)
				self.serverItemDict[serverID] = item
				dy += 30
			dy += 16
		else:
			self.serverMark = None

		self.__MakeText(self, self.__X(dx), self.__Y(dy), localeInfo.CHANNEL_SELECT_CHANNEL, 0xffc3ccd6, 11)
		self.channelDY = dy + 26
		self.channelRing = self.__MakeImage("dot_ring.tga", dx, self.channelDY)
		self.channelRing.Hide()

		self.selectedChannelID = loadChannelID
		self.__SelectServer(loadServerID)

	def __SelectServer(self, serverID):
		serverDict = self.__GetServerDict()
		if not serverDict.has_key(serverID):
			return

		self.selectedServerID = serverID

		for eachServerID, item in self.serverItemDict.items():
			if eachServerID == serverID:
				item.textLine.SetPackedFontColor(0xffffffff)
				if self.serverMark:
					self.serverMark.SetPosition(self.__X(self.blockDX - 20), self.__Y(item.posDY - 6))
			else:
				item.textLine.SetPackedFontColor(0xffc3ccd6)

		for item in self.channelItemDict.values():
			item.dot.Hide()
			item.Hide()
		self.channelItemDict = {}

		channelDict = serverDict[serverID].get("channel", {})
		channelIDList = channelDict.keys()
		channelIDList.sort()

		for index, channelID in enumerate(channelIDList):
			posDX = self.blockDX - 8 + index * 46

			item = ui.Window()
			item.SetParent(self)
			item.SetPosition(self.__X(posDX), self.__Y(self.channelDY - 8))
			item.SetSize(int(40 * self.unit), int(62 * self.unit))
			item.SetMouseLeftButtonDownEvent(ui.__mem_func__(self.__SelectChannel), channelID)
			item.Show()

			item.dot = self.__MakeImage("dot_none.tga", posDX + 8, self.channelDY)
			item.textLine = self.__MakeText(item, int(18 * self.unit), int(36 * self.unit), str(channelID), 0xffc3ccd6, 13)
			item.textLine.SetHorizontalAlignCenter()
			item.posDX = posDX
			self.channelItemDict[channelID] = item

		if not channelDict.has_key(self.selectedChannelID):
			self.selectedChannelID = channelIDList[0] if channelIDList else 0

		self.__SelectChannel(self.selectedChannelID)
		self.__RequestServerStateList()

	def __SelectChannel(self, channelID):
		if not self.channelItemDict.has_key(channelID):
			return

		self.selectedChannelID = channelID

		for eachChannelID, item in self.channelItemDict.items():
			if eachChannelID == channelID:
				item.textLine.SetPackedFontColor(0xffffffff)
				self.channelRing.SetPosition(self.__X(item.posDX), self.__Y(self.channelDY - 8))
				self.channelRing.Show()
			else:
				item.textLine.SetPackedFontColor(0xffc3ccd6)

	def __RequestServerStateList(self):
		try:
			channelDict = self.__GetServerDict()[self.selectedServerID]["channel"]
		except KeyError:
			return

		ServerStateChecker.Initialize()
		for channelID, channelDataDict in channelDict.items():
			ServerStateChecker.AddChannel(channelDataDict["key"], channelDataDict["ip"], channelDataDict["udp_port"])

		ServerStateChecker.Request()

	def NotifyChannelState(self, addrKey, state):
		try:
			stateName = serverInfo.STATE_DICT[state]
		except KeyError:
			stateName = serverInfo.STATE_NONE

		channelID = addrKey % 10

		try:
			self.__GetServerDict()[self.selectedServerID]["channel"][channelID]["state"] = stateName
		except KeyError:
			return

		item = self.channelItemDict.get(channelID)
		if item:
			item.dot.SetImage({1 : "dot_norm.tga", 2 : "dot_busy.tga", 3 : "dot_full.tga"}.get(state, "dot_none.tga"))

	def __SaveChannelInfo(self):
		try:
			file = open("channel.inf", "w")
			file.write("%d %d %d" % (self.selectedServerID, self.selectedChannelID, 0))
		except:
			pass

	def __LoadChannelInfo(self):
		try:
			tokens = open("channel.inf").readlines()[0].split()
			return int(tokens[0]), int(tokens[1])
		except:
			return 0, 0

	def __ApplyConnectInfo(self):
		serverDict = self.__GetServerDict()

		try:
			serverData = serverDict[self.selectedServerID]
			channelData = serverData["channel"][self.selectedChannelID]
		except KeyError:
			self.PopupNotifyMessage(localeInfo.CHANNEL_SELECT_CHANNEL)
			return False

		if channelData["state"] == serverInfo.STATE_DICT[3]:
			self.PopupNotifyMessage(localeInfo.CHANNEL_NOTIFY_FULL)
			return False

		self.__SaveChannelInfo()

		net.SetServerInfo(("%s, %s" % (serverData.get("name", ""), channelData["name"])).strip())

		try:
			accountData = serverInfo.REGION_AUTH_SERVER_DICT[0][self.selectedServerID]
			accountIP = accountData["ip"]
			accountPort = accountData["port"]
		except KeyError:
			accountIP = 0
			accountPort = 0

		markAddrValue = serverInfo.MARKADDR_DICT.get(self.selectedServerID * 10)
		if markAddrValue:
			net.SetMarkServer(markAddrValue["ip"], markAddrValue["tcp_port"])
			app.SetGuildMarkPath(markAddrValue["mark"])
			app.SetGuildSymbolPath(markAddrValue["symbol_path"])

		self.stream.SetConnectInfo(channelData["ip"], channelData["tcp_port"], accountIP, accountPort)
		return True

	def __ExitGame(self):
		app.Exit()

	def SetIDEditLineFocus(self):
		if self.idEditLine != None:
			self.idEditLine.SetFocus()

	def SetPasswordEditLineFocus(self):
		if self.idEditLine != None:
			self.idEditLine.SetText("")
			self.idEditLine.SetFocus()

		if self.pwdEditLine != None:
			self.pwdEditLine.SetText("")

	def OnEndCountDown(self):
		self.isNowCountDown = False
		self.OnConnectFailure()

	def OnConnectFailure(self):
		if self.isNowCountDown:
			return

		snd.PlaySound("sound/ui/loginfail.wav")

		if self.connectingDialog:
			self.connectingDialog.Close()
		self.connectingDialog = None

		self.PopupNotifyMessage(localeInfo.LOGIN_CONNECT_FAILURE, self.SetPasswordEditLineFocus)

	def OnHandShake(self):
		if not IsLoginDelay():
			snd.PlaySound("sound/ui/loginok.wav")
			self.PopupDisplayMessage(localeInfo.LOGIN_CONNECT_SUCCESS)

	def OnLoginStart(self):
		if not IsLoginDelay():
			self.PopupDisplayMessage(localeInfo.LOGIN_PROCESSING)

	def OnLoginFailure(self, error):
		if self.connectingDialog:
			self.connectingDialog.Close()
		self.connectingDialog = None

		try:
			loginFailureMsg = self.loginFailureMsgDict[error]
		except KeyError:
			loginFailureMsg = PASSPOD_MSG_DICT.get(error, localeInfo.LOGIN_FAILURE_UNKNOWN + error)

		loginFailureFunc = self.loginFailureFuncDict.get(error, self.SetPasswordEditLineFocus)
		self.PopupNotifyMessage(loginFailureMsg, loginFailureFunc)

		snd.PlaySound("sound/ui/loginfail.wav")

	def __DisconnectAndInputPassword(self):
		if self.connectingDialog:
			self.connectingDialog.Close()
		self.connectingDialog = None

		self.SetPasswordEditLineFocus()
		net.Disconnect()

	def Connect(self, id, pwd):
		if constInfo.SEQUENCE_PACKET_ENABLE:
			net.SetPacketSequenceMode()

		if IsLoginDelay():
			loginDelay = GetLoginDelay()
			self.connectingDialog = ConnectingDialog()
			self.connectingDialog.Open(loginDelay)
			self.connectingDialog.SAFE_SetTimeOverEvent(self.OnEndCountDown)
			self.connectingDialog.SAFE_SetExitEvent(self.OnPressExitKey)
			self.isNowCountDown = True
		else:
			self.stream.popupWindow.Close()
			self.stream.popupWindow.Open(localeInfo.LOGIN_CONNETING, self.SetPasswordEditLineFocus, localeInfo.UI_CANCEL)

		uiTarget.MONSTER_INFO_DATA.clear()
		self.stream.SetLoginInfo(id, pwd)
		self.stream.Connect()

	def __LoadLoginInfo(self, loginInfoFileName):
		loginInfo = {}
		try:
			execfile(loginInfoFileName, loginInfo)
		except IOError:
			pass

		id = loginInfo.get("id", "")
		pwd = loginInfo.get("pwd", "")
		addr = loginInfo.get("addr", "")
		port = loginInfo.get("port", 0)
		account_addr = loginInfo.get("account_addr", addr)
		account_port = loginInfo.get("account_port", port)

		self.stream.SetCharacterSlot(loginInfo.get("slot", 0))
		self.stream.isAutoLogin = loginInfo.get("autoLogin", 0)
		self.stream.isAutoSelect = loginInfo.get("autoSelect", 0)
		app.loggined = FALSE

		if addr and port:
			net.SetMarkServer(addr, port)
			net.SetServerInfo(addr + ":" + str(port))
			self.stream.SetConnectInfo(addr, port, account_addr, account_port)

			if self.stream.isAutoLogin:
				self.Connect(id, pwd)

	def PopupDisplayMessage(self, msg):
		self.stream.popupWindow.Close()
		self.stream.popupWindow.Open(msg)

	def PopupNotifyMessage(self, msg, func = 0):
		if not func:
			func = self.EmptyFunc

		self.stream.popupWindow.Close()
		self.stream.popupWindow.Open(msg, func, localeInfo.UI_OK)

	def OnPressExitKey(self):
		self.stream.popupWindow.Close()
		self.stream.SetPhaseWindow(0)
		return True

	def OnExit(self):
		self.stream.popupWindow.Close()

	def OnUpdate(self):
		ServerStateChecker.Update()

		if not self.idEditLine:
			return

		if self.rotateIndex >= 0:
			self.__UpdateRotation()
			if not self.idEditLine:
				return

		focusState = 1 if self.pwdEditLine.IsFocus() else 0
		if focusState != self.focusState:
			self.focusState = focusState
			self.userField.SetImage("field_user_0.tga" if focusState else "field_user_1.tga")
			self.lockField.SetImage("field_lock_1.tga" if focusState else "field_lock_0.tga")

	def EmptyFunc(self):
		pass

	def __OnClickLoginButton(self):
		id = self.idEditLine.GetText()
		pwd = self.pwdEditLine.GetText()

		if len(id) == 0:
			self.PopupNotifyMessage(localeInfo.LOGIN_INPUT_ID, self.SetIDEditLineFocus)
			return

		if len(pwd) == 0:
			self.PopupNotifyMessage(localeInfo.LOGIN_INPUT_PASSWORD, self.SetPasswordEditLineFocus)
			return

		if not self.__ApplyConnectInfo():
			return

		self.Connect(id, pwd)

	def SameLogin_OpenUI(self):
		self.stream.popupWindow.Close()
