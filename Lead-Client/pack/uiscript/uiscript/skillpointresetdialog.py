ROOT = "d:/ymir work/ui/public/"

window = {
	"name" : "SkillResetDialog",
	"style" : ("float",),

	"x" : SCREEN_WIDTH/2 - 100,
	"y" : SCREEN_HEIGHT/2 - 114,

	"width" : 200,
	"height" : 228,

	"children" :
	(
		{
			"name" : "board",
			"type" : "thinboard",

			"x" : 0,
			"y" : 0,

			"width" : 200,
			"height" : 98,

			"children" :
			(
				{
					"name" : "reset_button",
					"type" : "button",

					"x" : 10,
					"y" : 17,

					"text" : "Reset Skill Points",

					"default_image" : ROOT + "XLarge_Button_01.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_03.sub",
				},

				{
					"name" : "cancel_button",
					"type" : "button",

					"x" : 10,
					"y" : 57,

					"text" : "Cancel",

					"default_image" : ROOT + "XLarge_Button_01.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_03.sub",
				},
			),
		},
	),
}