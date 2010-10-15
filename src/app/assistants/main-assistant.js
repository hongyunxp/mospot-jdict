var FmtUtil = {
	_LfToBr : function(text) {
		return text? text.replace(/\\n/g, "<br />") : null;
	},
	_texToHtmlMarks : function(text, marks) {
		for(var i = 0; i < marks.length; ++i) {
			text = text? text.replace(RegExp("\\\\"+marks[i]+"\\{(.+?)\\}", "g"), '<div class="'+marks[i]+'">$1</div>') : null;
		}
		return text;
	},
	_texToHtml : function(text) {
		return text? text.replace(/\\([a-zA-Z_]+?)\{(.+?)\}/g, '<div class="$1">$2</div>') : null;
	}
};
var _dumpObj = function(obj) {
	Mojo.Log.info("#######################obj:", obj? obj.toString() : "<undefined>");
	for(var key in obj)
	{
		Mojo.Log.info("####obj:", key, obj[key]);
	}
};
function MainAssistant() {
	/* this is the creator function for your scene assistant object. It will be passed all the 
	   additional parameters (after the scene name) that were passed to pushScene. The reference
	   to the scene controller (this.controller) has not be established yet, so any initialization
	   that needs the scene controller should be done in the setup function below. */
}

MainAssistant.prototype.setup = function() {
	/* this function is for setup tasks that have to happen when the scene is first created */
	this.controller.window.PalmSystem.setWindowOrientation("free");
	this.dict = { rowid: 0, count: 10 /* TODO: read dict count */ };
	this.dict.db = openDatabase("langdao-ec-gb", 1);

	/* use Mojo.View.render to render view templates and add them to the scene, if needed */
	this.initDictTemplate();
	
	/* setup widgets here */
	this.controller.setInitialFocusedElement(null);
	this.searchField = this.controller.get("searchInput");
	
	
	this.controller.setupWidget("dict-selector",
		this.attributes = {
			choices: [
				{label: "One", value: 1},
				{label: "Two", value: 2},
				{label: "Three", value: 3}
			]},
		this.model = {
			value: 3,
			disabled: false
		}
	);
	
	
	this.controller.setupWidget(Mojo.Menu.commandMenu,
		this.attributes = {
		/*	spacerHeight: 0, */
			menuClass: 'no-fade'
		},
		this.model = {
			visible: true,
			items: [ 
				{ icon: "back", command: "look-Previous" },
				{ icon: 'forward', command: 'look-Next' }
			]
		}
	);
	
	/* add event handlers to listen to events from widgets */
	this.controller.listen(this.controller.sceneElement, Mojo.Event.keydown, this.handleKeyDown.bind(this));
	this.controller.listen(this.searchField, "input", this.handleInput.bind(this));
	this.controller.listen(this.searchField, Mojo.Event.filter, this.handleFilter.bind(this));
	this.controller.listen(this.searchField, "focus", this.handleFocus.bind(this));
	this.controller.listen(this.searchField, "keydown", this.handleEnter.bind(this));
};

MainAssistant.prototype.activate = function(event) {
	/* put in event handlers here that should only be in effect when this scene is active. For
	   example, key handlers that are observing the document */
};

MainAssistant.prototype.deactivate = function(event) {
	/* remove any event handlers you added in activate and do any other cleanup that should happen before
	   this scene is popped or another scene is pushed on top */
};

MainAssistant.prototype.cleanup = function(event) {
	/* this function should do any cleanup needed before the scene is destroyed as 
	   a result of being popped off the scene stack */
	this.controller.stopListening(this.controller.sceneElement, Mojo.Event.keydown, this.handleKeyDown.bind(this));
	this.controller.stopListening(this.searchField, "input", this.handleInput.bind(this));
	this.controller.stopListening(this.searchField, Mojo.Event.filter, this.handleFilter.bind(this));
	this.controller.stopListening(this.searchField, "focus", this.handleFocus.bind(this));
	this.controller.stopListening(this.searchField, "keydown", this.handleEnter.bind(this));
};

///////////////////////////////////////////
MainAssistant.prototype.handleEnter = function(event) {
	if (Mojo.Char.isEnterKey(event.keyCode)) {
		this.searchField.select();
		// TODO: if already selected, move to next dict
	}
};
MainAssistant.prototype.handleFocus = function(event) {
	this.searchField.select();
};
MainAssistant.prototype.handleKeyDown = function(event) {
	if(event.originalEvent.keyCode != Mojo.Char.metaKey && event.originalEvent.keyCode != Mojo.Char.escape) {
		this.searchField.focus();
	}
};
MainAssistant.prototype.handleInput = function(event) {
	if(this.filterString != this.searchField.value) {
		this.filterString = this.searchField.value;
		Mojo.Event.send(this.searchField, Mojo.Event.filter, { filterString: this.searchField.value });
	}
};
MainAssistant.prototype.handleFilter = function(event) {
	this.lookUp(event.filterString);
};

MainAssistant.prototype.handleCommand = function(event) {
	if(event.type === Mojo.Event.command) {
		switch (event.command) {
		case "look-Previous":
			this.lookNext(-1);
			break;
		case "look-Next":
			this.lookNext(1);
			break;
		}
	} else if(event.type == Mojo.Event.back) {
		this.searchField.blur();
	}
};
//////////////////
MainAssistant.prototype.lookUp = function(word) {
	var keyword = word.replace(/\'/g, "''");
	var sqlQuery = "Select rowid,* from dict where word>='" + keyword + "' collate nocase limit 1";
	this.dict.db.transaction((function(transaction) {
			transaction.executeSql(sqlQuery, [],
								   this.handleTransactionSucces.bind(this),
								   this.handleTransactionError.bind(this));
		}).bind(this));	
};
MainAssistant.prototype.lookNext = function(offset) {
	this.dict.rowid = parseInt(this.dict.rowid) + offset;
	if(this.dict.rowid < 0) this.dict.rowid = 0;
	if(this.dict.rowid > this.dict.count) this.dict.rowid = this.dict.count;
	var sqlQuery = "Select rowid,* from dict where rowid=" + this.dict.rowid;
	this.dict.db.transaction((function(transaction) {
			transaction.executeSql(sqlQuery, [],
								   this.handleTransactionSucces.bind(this),
								   this.handleTransactionError.bind(this));
		}).bind(this));	
};
MainAssistant.prototype.handleTransactionSucces = function(transaction, SQLResultSet) {
	if(SQLResultSet.rows.length > 0) {
		var dictModel = SQLResultSet.rows.item(0);
		this.dict.renderParams.object = dictModel;
		this.controller.get("dict-main").innerHTML = Mojo.View.render(this.dict.renderParams);
		
		this.dict.rowid = dictModel.rowid;
		this.dict.renderParams.object = null;
	}
};
MainAssistant.prototype.handleTransactionError = function(transaction, error) {
	Mojo.Log.error("sql error: ", error.message);
};

MainAssistant.prototype.initDictTemplate = function() {
	var template = '<div class="word">#{-word}</div><div class="pron">#{-pron}</div><div class="expl">#{-expl}</div><div class="sent">#{-sent}</div>';
	this.dict.meta = { template : template };
	this.dict.renderParams = { };
	this.dict.db.transaction((function(transaction) {
			transaction.executeSql("select * from meta", [],
								this.handleMetaSuccess.bind(this),
								this.handleTransactionError.bind(this));
		}).bind(this));
};

MainAssistant.prototype.handleMetaSuccess = function(transaction, SQLResultSet) {
	// update dict meta
	for(var i = 0; i < SQLResultSet.rows.length; ++i) {
		var item = SQLResultSet.rows.item(i);
		this.dict.meta[item.key] = item.value;
	}
	// setup styles
	this.controller.get("dict-style").outerHTML = '<style type="text/css" id="dict-style">'+ this.dict.meta.style +'</style>';
	
	// setup render params
	this.dict.renderParams = { inline: this.dict.meta.template,
		formatters: this.dict.meta.formatters? eval('('+this.dict.meta.formatters+')') : undefined
	};
};



