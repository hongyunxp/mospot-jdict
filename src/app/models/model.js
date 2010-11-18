Model = ({
	model: {
		orientation: "up",
		fontSize: "medium",
		dictIndex: 0,
		word: ""
	},
	
	init: function() {
		this.cookie = new Mojo.Model.Cookie("Prefs");
		var prefData = this.cookie.get();
		if(prefData) {
			this.model.orientation = prefData.orientation || this.model.orientation;
			this.model.fontSize = prefData.fontSize || this.model.fontSize;
			this.model.dictIndex = prefData.dictIndex || this.model.dictIndex;
			this.model.word = prefData.word || this.model.word;
		}
	},
	store: function() {
		this.cookie.put( {
			orientation: this.model.orientation,
			fontSize: this.model.fontSize,
			dictIndex: this.model.dictIndex,
			word: this.model.word
		});
	}
	
});

