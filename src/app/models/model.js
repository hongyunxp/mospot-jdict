Model = ({
	model: {
		fontSize: "medium",
		dictIndex: 0,
		word: ""
	},
	
	init: function() {
		this.cookie = new Mojo.Model.Cookie("Prefs");
		var prefData = this.cookie.get();
		if(prefData) {
			this.model.fontSize = prefData.fontSize;
			this.model.dictIndex = prefData.dictIndex;
			this.model.word = prefData.word;
		}
	},
	store: function() {
		this.cookie.put( {
			fontSize: this.model.fontSize,
			dictIndex: this.model.dictIndex,
			word: this.model.word
		});
	}
	
});

