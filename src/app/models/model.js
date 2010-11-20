Model = ({
	model: {
		orientation: "up",
		fontSize: "100%",
		caseSensitive: false,
		dictIndex: 0,
		word: ""
	},
	
	init: function() {
		this.cookie = new Mojo.Model.Cookie("Prefs");
		var prefData = this.cookie.get();
		if(prefData) {
			this.model.orientation = prefData.orientation || this.model.orientation;
			this.model.fontSize = prefData.fontSize || this.model.fontSize;
			this.model.caseSensitive = prefData.caseSensitive || this.model.caseSensitive;
			this.model.dictIndex = prefData.dictIndex || this.model.dictIndex;
			this.model.word = prefData.word || this.model.word;
		}
	},
	store: function() {
		this.cookie.put(this.model);
	}
	
});

