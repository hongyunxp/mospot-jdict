function MainAssistant() {
	/* this is the creator function for your scene assistant object. It will be passed all the 
	   additional parameters (after the scene name) that were passed to pushScene. The reference
	   to the scene controller (this.controller) has not be established yet, so any initialization
	   that needs the scene controller should be done in the setup function below. */
}

MainAssistant.prototype.setup = function() {
	/* this function is for setup tasks that have to happen when the scene is first created */
	Model.init();

	/* use Mojo.View.render to render view templates and add them to the scene, if needed */
	this.initDicts();
	
	/* setup widgets here */
	this.controller.setInitialFocusedElement(null);
	this.searchField = this.controller.get("search-input");
	this.searchField.value = Model.model.word;
	this.controller.setupWidget("dict-selector",
			{ modelProperty: "dictIndex", choices: this.dictNames = [] },
			Model.model);
	// command menus
	this.controller.setupWidget(Mojo.Menu.commandMenu,
			{ menuClass: 'no-fade' },
			this.commandMenuModel = { visible: true,
				items: [ {},
					{ icon: "back", command: "lookPrevious" },
					{ icon: 'forward', command: 'lookNext' } ] } );
	// app menu
	Mojo.Menu.prefsItem.checkEnabled = false;
	Mojo.Menu.helpItem.checkEnabled = false;
	
	// update dict-selector model in initDictsComplete callback
	//   setup dict style after up date dict-selector model
	
	/* add event handlers to listen to events from widgets */
	this.keyDownEventListener = this.handleKeyDown.bindAsEventListener(this);
	this.inputEventListener = this.handleInput.bindAsEventListener(this);
	this.filterEventListener = this.handleFilter.bindAsEventListener(this);
	this.focusEventListener = this.handleFocus.bindAsEventListener(this);
	this.enterEventListener = this.handleEnter.bindAsEventListener(this);
	this.docDeactivateEventListener = this.handleDocDeactivate.bindAsEventListener(this);
	this.dictSelectEventListener = this.handleDictSelect.bindAsEventListener(this);
	this.controller.listen(this.controller.sceneElement, Mojo.Event.keydown, this.keyDownEventListener);
	this.controller.listen(this.searchField, "input", this.inputEventListener);
	this.controller.listen(this.searchField, Mojo.Event.filter, this.filterEventListener);
	this.controller.listen(this.searchField, "focus", this.focusEventListener);
	this.controller.listen(this.searchField, "keydown", this.enterEventListener);
	this.controller.listen(this.controller.document, Mojo.Event.stageDeactivate, this.docDeactivateEventListener, false);
	this.controller.listen("dict-selector", Mojo.Event.propertyChange, this.dictSelectEventListener);
};

MainAssistant.prototype.activate = function(event) {
	/* put in event handlers here that should only be in effect when this scene is active. For
	   example, key handlers that are observing the document */
	this.controller.get("main").style.fontSize = Model.model.fontSize;
	//this.controller.window.PalmSystem.setWindowOrientation(Model.model.orientation);
	this.controller.stageController.setWindowOrientation(Model.model.orientation);
	this.searchField.select();
	this.searchField.blur();
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
	
	Model.store();
};

MainAssistant.prototype.aboutToActivate = function(continueActivate) {
	Mojo.Log.error("aboutToActivate:", this.pendingInits);
	if(this.pendingInits && this.pendingInits > 0) {
		this.continueActivate = continueActivate;
	} else {
		continueActivate();
	}
};
///////////////////////////////////////////
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
	if(Model.model.word != this.searchField.value) {
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
};
MainAssistant.prototype.handleDictSelect = function(event) {
	this.dicts[event.value].load(this.loadDictComplete.bind(this));
};
MainAssistant.prototype.handleCommand = function(event) {
	if(event.type == Mojo.Event.command) {
		switch (event.command) {
		case "lookPrevious":
			this.lookNext(-1);
			break;
		case "lookNext":
			this.lookNext(1);
			break;
		case Mojo.Menu.prefsCmd:
			this.controller.stageController.pushScene("prefs");
			break;
		case Mojo.Menu.helpCmd:
			this.controller.stageController.pushScene("help");
			break;
		}
	}
};
//////////////////
MainAssistant.prototype.onLookUp = function(dictRenderParams) {
	this.controller.get("dict-main").innerHTML = Mojo.View.render(dictRenderParams);
	this.controller.getSceneScroller().mojo.revealTop();

	if(dictRenderParams.object.rowid <= 1) {
		this.commandMenuModel.items[1].disabled = true;
	} else if(dictRenderParams.object.rowid >= this.dicts[Model.model.dictIndex].count) {
		this.commandMenuModel.items[2].disabled = true;
	} else {
		this.commandMenuModel.items[1].disabled = false;
		this.commandMenuModel.items[2].disabled = false;
	}
	this.controller.modelChanged(this.commandMenuModel, this);
};
MainAssistant.prototype.lookUp = function(word) {
	Model.model.word = word;
	this.dicts[Model.model.dictIndex].lookUp(word, this.onLookUp.bind(this));
};
MainAssistant.prototype.onLookNext = function(dictRenderParams) {
	Model.model.word = dictRenderParams.object.word;
	this.searchField.value = Model.model.word;
	this.searchField.select();
	this.searchField.blur();
	this.onLookUp(dictRenderParams);
};
MainAssistant.prototype.lookNext = function(offset) {
	this.dicts[Model.model.dictIndex].lookNext(offset, this.onLookNext.bind(this));
};
//////////////////////
MainAssistant.prototype.loadDictComplete = function(meta) {
	// setup styles
	//this.controller.get("dict-style").outerHTML = '<style type="text/css" id="dict-style">'+ dict.meta.styles +'</style>';
	this.controller.get("dict-style").outerHTML = this.controller.get("dict-style").outerHTML.replace("{}", meta.styles);
	
	this.lookUp(Model.model.word);
};
MainAssistant.prototype.initDictsComplete = function() {
	// wait untill all dicts inited
	--this.pendingInits;
	if(this.pendingInits > 0) { return; }
	
	// all dicts inited
	delete this.pendingInits;

	var i, j;

	// remove invalid dicts
	for(i = 0; i < this.dicts.length; ++i) {
		if(!this.dicts[i].db) { this.dicts[i] = null; }
	}
	
	// setup dict_selector model
	for(i = 0, j = 0; i < this.dicts.length; ++i) {
		if(this.dicts[i]) {
			this.dictNames[j] = { label: this.dicts[i].name, value: i };
			++j;
		}
	}
	// if curent dict not valid, use the first
	if(Model.model.dictIndex < 0 || Model.model.dictIndex > this.dicts.length || !this.dicts[Model.model.dictIndex]) {
		Model.model.dictIndex = this.dictNames[0].value;
	}
	this.controller.modelChanged(Model.model, this);
	
	// load dict meta and render parameters
	this.dicts[Model.model.dictIndex].load(this.loadDictComplete.bind(this));
	
	// Continue activation procedure if we delayed it.
	if(this.continueActivate) {
		this.continueActivate();
		delete this.continueActivate;
	}
};
MainAssistant.prototype.initDicts = function() {
	this.dicts = [];
	this.MAX_DICTS = 7;
	this.pendingInits = this.MAX_DICTS;
	
	for(var i = 0; i < this.MAX_DICTS; ++i) {
		this.dicts[i] = new Dictionary("dict"+i);
		this.dicts[i].init(this.initDictsComplete.bind(this));
	}
};
