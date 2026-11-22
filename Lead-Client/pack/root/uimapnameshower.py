import app
import grp
import ui
import localeInfo

class MapNameShower(ui.Window):

	FONT_SIZE = 30
	TEXT_COLOR = (1.0, 0.95, 0.8)

	STATE_HIDE = 0
	STATE_FADE_IN = 1
	STATE_SHOW = 2
	STATE_FADE_OUT = 3

	def __init__(self):
		ui.Window.__init__(self, "TOP_MOST")
		self.AddFlag("not_pick")

		self.textLine = ui.TextLine()
		self.textLine.SetParent(self)
		self.textLine.AddFlag("not_pick")
		self.textLine.SetFontName("%s:%d" % (localeInfo.UI_DEF_FONT.split(":")[0], self.FONT_SIZE))
		self.textLine.SetHorizontalAlignCenter()
		self.textLine.SetOutline()
		self.textLine.Show()

		self.__Initialize()

	def __del__(self):
		ui.Window.__del__(self)

	def __Initialize(self):
		self.fadeStartTime = 0
		self.state = self.STATE_HIDE
		self.curAlpha = 0.0
		self.SetSize(0, 0)
		self.SetWindowHorizontalAlignCenter()
		self.SetPosition(0, 110)
		self.__SetAlpha(0.0)
		self.Hide()

	def __SetAlpha(self, alpha):
		red, green, blue = self.TEXT_COLOR
		self.textLine.SetPackedFontColor(grp.GenerateColor(red, green, blue, max(0.0, min(1.0, alpha))))

	def __GetDevilTowerFloor(self, x, y):
		if x > 10000 and y > 58000 and x < 25000 and y < 72000:
			return 1
		elif x > 10000 and y > 35000 and x < 25000 and y < 50000:
			return 2
		elif x > 10000 and y > 10000 and x < 25000 and y < 25000:
			return 3
		elif x > 35000 and y > 61000 and x < 43500 and y < 70500:
			return 4
		elif x > 35000 and y > 38000 and x < 43500 and y < 48000:
			return 5
		elif x > 14000 and y > 14000 and x < 43500 and y < 24500:
			return 6
		elif x > 56000 and y > 60000 and x < 68000 and y < 73000:
			return 7
		elif x > 56000 and y > 38000 and x < 68000 and y < 49000:
			return 8
		elif x > 56000 and y > 13000 and x < 68000 and y < 23000:
			return 9
		return 0

	def __GetDevilBase(self, x, y):
		if x > 3000 and y > 4500 and x < 45000 and y < 45000:
			return 1
		elif x > 54000 and y > 3900 and x < 100000 and y < 46200:
			return 2
		elif x > 104800 and y > 3500 and x < 145500 and y < 45800:
			return 3
		elif x > 3100 and y > 54100 and x < 56400 and y < 105800:
			return 4
		elif x > 65000 and y > 54000 and x < 105000 and y < 95500:
			return 5
		elif x > 117500 and y > 57600 and x < 142000 and y < 81000:
			return 6
		elif x > 5000 and y > 104900 and x < 15000 and y < 122000:
			return 7
		return 0

	def ShowMapName(self, mapName, x, y):
		name = localeInfo.MAP_NAME_DICT.get(mapName)
		if not name:
			return

		floor = 0
		if mapName == "metin2_map_deviltower1":
			floor = self.__GetDevilTowerFloor(x, y)
		elif mapName == "metin2_map_devilsCatacomb":
			floor = self.__GetDevilBase(x, y)

		if floor:
			name = "%s  %d" % (name, floor)

		self.__Initialize()
		self.textLine.SetText(name)

		self.state = self.STATE_FADE_IN
		self.fadeStartTime = app.GetTime() + 1.0
		self.Show()

	def Update(self):
		self.__SetAlpha(self.curAlpha)

		if self.STATE_FADE_IN == self.state:
			if app.GetTime() > self.fadeStartTime:
				self.curAlpha += 0.05

				if self.curAlpha > 0.9:
					self.state = self.STATE_SHOW
					self.fadeStartTime = app.GetTime() + 5.0

		elif self.STATE_SHOW == self.state:
			if app.GetTime() > self.fadeStartTime:
				self.state = self.STATE_FADE_OUT

		elif self.STATE_FADE_OUT == self.state:
			self.curAlpha -= 0.05

			if self.curAlpha < 0.0001:
				self.Hide()
