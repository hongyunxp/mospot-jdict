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
///////////////////////////

function MainAssistant() {
	/* this is the creator function for your scene assistant object. It will be passed all the 
	   additional parameters (after the scene name) that were passed to pushScene. The reference
	   to the scene controller (this.controller) has not be established yet, so any initialization
	   that needs the scene controller should be done in the setup function below. */
}

MainAssistant.prototype.loadDictComplete = function(dict) {
	// setup styles
	this.controller.get("dict-style").outerHTML = '<style type="text/css" id="dict-style">'+ dict.meta.style +'</style>';
};
MainAssistant.prototype.initDictsComplete = function() {
	--this.pendingInits;
	if(this.pendingInits > 0) return;
	
	// all dicts inited
	delete this.pendingInits;

	// remove invalid dicts
	for(var i = 0; i < this.dicts.length; ++i) {
		if(!this.dicts[i].db) this.dicts[i] = null;
	}

	// setup dict_selector model
	for(var i = 0, j = 0; i < this.dicts.length; ++i) {
		if(this.dicts[i]) {
			this.dictSelectorModel.choices[j] = { label: this.dicts[i].name, value: i };
			this.dictSelectorModel.value = i;
			++j;
		}
	}
	for(var i = 0; i < this.dictSelectorModel.choices.length; ++i) {
		//if(this.dictSelectorModel.choices[i].value == this.dictSelectorModel.value);
	}
	this.controller.modelChanged(this.dictSelectorModel, this);
	Mojo.Log.info("this.dictSelectorModel.value:", this.dictSelectorModel.value);
	
	// load dict meta and render parameters
	this.dicts[this.dictSelectorModel.value].load(this.loadDictComplete.bind(this));
	
	// Continue activation procedure if we delayed it.
	if(this.continueActivate) {
		this.continueActivate();
		delete this.continueActivate;
	}
}
MainAssistant.prototype.initDicts = function() {
	this.dicts = [];
	this.MAX_DICTS = 7;
	this.pendingInits = this.MAX_DICTS;
	
	for(var i = 0; i < this.MAX_DICTS; ++i) {
		this.dicts[i] = new Dictionary("dict"+i);
		this.dicts[i].init(this.initDictsComplete.bind(this));
	}
}
MainAssistant.prototype.setup = function() {
	/* this function is for setup tasks that have to happen when the scene is first created */
	// this.controller.window.PalmSystem.setWindowOrientation("free");

	/* use Mojo.View.render to render view templates and add them to the scene, if needed */
	this.initDicts();
	
	/* setup widgets here */
	this.controller.setInitialFocusedElement(null);
	this.searchField = this.controller.get("searchInput");
	this.controller.setupWidget("dict-selector",
		{ modelProperty: "value" },
		this.dictSelectorModel = { value: 0, choices: [] }
	);
	// update dictSelectorModel in initDictsComplete callback
	//   setup dict style after up date dictSelectorModel
	
	/* add event handlers to listen to events from widgets */
	this.aboutToActivateEventListener = this.handleAboutToActivate.bindAsEventListener(this);
	this.controller.listen(this.controller.sceneElement, Mojo.Event.aboutToActivate, this.aboutToActivateEventListener);
	
	this.keyDownEventListener = this.handleKeyDown.bindAsEventListener(this)
	this.inputEventListener = this.handleInput.bindAsEventListener(this);
	this.filterEventListener = this.handleFilter.bindAsEventListener(this);
	this.focusEventListener = this.handleFocus.bindAsEventListener(this);
	this.enterEventListener = this.handleEnter.bindAsEventListener(this);
	this.docDeactivateEventListener = this.handleDocDeactivate.bindAsEventListener(this);
	this.controller.listen(this.controller.sceneElement, Mojo.Event.keydown, this.keyDownEventListener);
	this.controller.listen(this.searchField, "input", this.inputEventListener);
	this.controller.listen(this.searchField, Mojo.Event.filter, this.filterEventListener);
	this.controller.listen(this.searchField, "focus", this.focusEventListener);
	this.controller.listen(this.searchField, "keydown", this.enterEventListener);
	this.controller.listen(this.controller.document, Mojo.Event.stageDeactivate, this.docDeactivateEventListener, false);
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
	this.controller.stopListening(this.controller.sceneElement, Mojo.Event.keydown, this.keyDownEventListener);
	this.controller.stopListening(this.searchField, "input", this.inputEventListener);
	this.controller.stopListening(this.searchField, Mojo.Event.filter, this.filterEventListener);
	this.controller.stopListening(this.searchField, "focus", this.focusEventListener);
	this.controller.stopListening(this.searchField, "keydown", this.enterEventListener);
	this.controller.stopListening(this.controller.document, Mojo.Event.stageDeactivate, this.docDeactivateEventListener, false);
};

///////////////////////////////////////////
MainAssistant.prototype.handleAboutToActivate = function(event) {
	this.controller.stopListening(this.controller.sceneElement, Mojo.Event.aboutToActivate, this.aboutToActivateEventListener);
	if(this.pendingInits > 0) {
		this.continueActivate = event.synchronizer.wrap(Mojo.doNothing);
	}
}
MainAssistant.prototype.handleEnter = function(event) {
	if (Mojo.Char.isEnterKey(event.keyCode)) {
		this.searchField.select();
		// TODO: if already selected, move to next dict
	}
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
MainAssistant.prototype.handleFocus = function(event) {
	// this.searchField.select();
};
MainAssistant.prototype.handleDocDeactivate = function(event) {
	this.searchField.blur();
}
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
	} 
};
//////////////////
MainAssistant.prototype.onLookUp = function(dict) {
	if(dict) {
		this.controller.get("dict-main").innerHTML = Mojo.View.render(dict.renderParams);
	}
}
MainAssistant.prototype.lookUp = function(word) {
	this.dicts[this.dictSelectorModel.value].lookUp(word, this.onLookUp.bind(this));
};
MainAssistant.prototype.lookNext = function(offset) {
	this.dicts[this.dictSelectorModel.value].lookNext(offset, this.onLookUp.bind(this));
};
