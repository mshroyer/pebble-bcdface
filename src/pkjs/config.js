module.exports = [
  {
    "type": "heading",
    "defaultValue": "App Configuration"
  },
  {
    "type": "section",
    "items": [
      {
	"type": "toggle",
	"messageKey": "SecondTick",
	"label": "Enable Seconds",
	"defaultValue": false
      },
      {
	"type": "toggle",
	"messageKey": "NotifyDisconnect",
	"label": "Notify on Phone Disconnect",
	"defaultValue": true
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
]
