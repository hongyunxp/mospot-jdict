Model = ({
	model: {
		fontSize: "normal",
		choices: [], // will not save, for dict-selector
		dictIndex: 0,
		word: "hello"
	},
	
	init: function() {
		this.cookie = new Mojo.Model.Cookie("Prefs");
		var prefData = this.cookie.get();
		if(prefData) {
			this.model = prefData;
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

