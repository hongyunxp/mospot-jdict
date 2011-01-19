function MainAssistant() {
	/* this is the creator function for your scene assistant object. It will be passed all the 
	   additional parameters (after the scene name) that were passed to pushScene. The reference
	   to the scene controller (this.controller) has not be established yet, so any initialization
	   that needs the scene controller should be done in the setup function below. */
}

MainAssistant.prototype.HandleDragStart = function(event) {
	event.down.pageX = event.move.pageX;
	event.down.clientX = event.move.clientX;
	event.down.pageY = event.move.pageY;
	event.down.clientY = event.move.clientY;
	//event.down.target = event.move.target;
};

MainAssistant.prototype.setup = function() {
	Mojo.Log.error("MainAssistant.prototype.setup");
	/* this function is for setup tasks that have to happen when the scene is first created */
	this.pendingLoads = 1;
	$("plugin").ready = this.initDicts.bind(this);

	Model.init();

	/* use Mojo.View.render to render view templates and add them to the scene, if needed */
	
	/* setup widgets here */
	this.controller.setInitialFocusedElement();
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
	// swipe scroller
	this.controller.setupWidget("swipe-scroller", { mode: 'horizontal-snap' },
			this.swipeModel = {
				snapElements: { x: $$('.swipe-item') },
				snapIndex: 1 } );
	this.swipeScroller = this.controller.get('swipe-scroller');
	
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
	this.controller.listen(this.controller.document, Mojo.Event.stageDeactivate, this.docDeactivateEventListener);
	this.controller.listen("dict-selector", Mojo.Event.propertyChange, this.dictSelectEventListener);

	this.dragStartEventListener = this.HandleDragStart.bindAsEventListener(this);
	this.controller.listen(this.controller.getSceneScroller(), Mojo.Event.dragStart, this.dragStartEventListener, true);
};

MainAssistant.prototype.activate = function(event) {
	Mojo.Log.error("MainAssistant.prototype.activate");
	/* put in event handlers here that should only be in effect when this scene is active. For
	   example, key handlers that are observing the document */
	this.controller.get("swipe-main").style.fontSize = Model.model.fontSize;
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
	this.controller.stopListening(this.controller.getSceneScroller(), Mojo.Event.dragStart, this.dragStartEventListener);

	this.controller.stopListening(this.controller.sceneElement, Mojo.Event.keydown, this.keyDownEventListener);
	this.controller.stopListening(this.searchField, "input", this.inputEventListener);
	this.controller.stopListening(this.searchField, Mojo.Event.filter, this.filterEventListener);
	this.controller.stopListening(this.searchField, "focus", this.focusEventListener);
	this.controller.stopListening(this.searchField, "keydown", this.enterEventListener);
	this.controller.stopListening(this.controller.document, Mojo.Event.stageDeactivate, this.docDeactivateEventListener);
	this.controller.stopListening("dict-selector", Mojo.Event.propertyChange, this.dictSelectEventListener);
	
	Model.store();
	this.freeDicts();
};

MainAssistant.prototype.aboutToActivate = function(continueActivate) {
	Mojo.Log.error("MainAssistant.prototype.aboutToActivate", this.pendingLoads);
	if(this.pendingLoads) {
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
	} else {
		// fix that an input method do not generate "input" event
		setTimeout(function(){ Mojo.Event.send(this.searchField, "input"); }.bind(this), 0);
	}
};
MainAssistant.prototype.handleKeyDown = function(event) {
	if(event.originalEvent.keyCode != Mojo.Char.metaKey && event.originalEvent.keyCode != Mojo.Char.escape) {
		this.searchField.focus();
	}
};
MainAssistant.prototype.handleInput = function(event) {
	if(Model.model.word != this.searchField.value) {
		Model.model.word = this.searchField.value;
		Mojo.Event.send(this.searchField, Mojo.Event.filter, { filterString: this.searchField.value });
	}
};
MainAssistant.prototype.handleFilter = function(event) {
	this.lookUp(event.filterString, Model.model.caseSensitive);
};
MainAssistant.prototype.handleFocus = function(event) {
	// this.searchField.select();
};
MainAssistant.prototype.handleDocDeactivate = function(event) {
	this.searchField.blur();
};
MainAssistant.prototype.handleDictSelect = function(event) {
	//this.dicts[event.oldValue].free();
	this.freeDicts();
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
	} else if(event.type == Mojo.Event.commandEnable &&
			(event.command == Mojo.Menu.prefsCmd || event.command == Mojo.Menu.helpCmd)) {
		event.stopPropagation();
	}

};
//////////////////
MainAssistant.prototype.onLookUp = function(row) {
	if(row) {
		this.dictRenderParams.object = row;
		this.controller.get("dict-main").innerHTML = Mojo.View.render(this.dictRenderParams);
		this.controller.getSceneScroller().mojo.revealTop();
	
		if(this.dictRenderParams.object.rowid <= 1) {
			this.commandMenuModel.items[1].disabled = true;
		} else if(this.dictRenderParams.object.rowid >= this.dicts[Model.model.dictIndex].count) {
			this.commandMenuModel.items[2].disabled = true;
		} else {
			this.commandMenuModel.items[1].disabled = false;
			this.commandMenuModel.items[2].disabled = false;
		}
		this.controller.modelChanged(this.commandMenuModel, this);
		this.dictRenderParams.object = undefined;
	} else {
		this.lookUp("");
	}
};
MainAssistant.prototype.lookUp = function(word, caseSense) {
	Mojo.Log.error("MainAssistant.prototype.lookUp", word, caseSense);
	this.dicts[Model.model.dictIndex].lookUp(word, this.onLookUp.bind(this), caseSense);
};
MainAssistant.prototype.onLookNext = function(row) {
	Model.model.word = row.word;
	this.searchField.value = Model.model.word;
	this.searchField.select();
	this.searchField.blur();
	this.onLookUp(row);
};
MainAssistant.prototype.lookNext = function(offset) {
	this.dicts[Model.model.dictIndex].lookNext(offset, this.onLookNext.bind(this));
};
//////////////////////
MainAssistant.prototype.freeDicts = function() {
	if(this.dicts) {
		for(var i = 0; i < this.dicts.length; ++i) {
			if(this.dicts[i].db) {
				this.dicts[i].free();
			}
		}
	}
};
MainAssistant.prototype.loadDictComplete = function(meta) {
	// setup render params
	this.dictRenderParams = { inline: meta.template,
		formatters: meta.formatters? eval('('+meta.formatters+')') : undefined
	};
	// setup styles
	this.controller.get("dict-main").innerHTML = "&nbsp;";
	this.lookUp(Model.model.word, Model.model.caseSensitive);
	this.controller.get("dict-style").outerHTML = this.controller.get("dict-style").outerHTML
		.replace(/(<style.*?>).*?(<\/style>)/, "$1"+meta.styles+"$2");
};
MainAssistant.prototype.queryDictsComplete = function(dicts) {
	Mojo.Log.error("queryDictsComplete");
	this.dicts = this.dicts.concat(dicts);
	_dumpObj(this.dicts);

	--this.pendingLoads;
	if(this.pendingLoads > 0) { return; }
	delete this.pendingLoads;

	// setup dict_selector model
	for(var i = 0; i < this.dicts.length; ++i) {
		this.dictNames[i] = { label: this.dicts[i].name, value: i };
	}
	// if curent dict not valid, use the first
	if(Model.model.dictIndex < 0 || Model.model.dictIndex >= this.dicts.length) {
		Model.model.dictIndex = 0;
	}
	this.controller.modelChanged(Model.model, this);
	
	Mojo.Log.error("load:", Model.model.dictIndex);
	_dumpObj(this.dicts[Model.model.dictIndex]);
	// load dict meta and render parameters
	this.dicts[Model.model.dictIndex].load(this.loadDictComplete.bind(this));
	
	// Continue activation procedure if we delayed it.
	if(this.continueActivate) {
		this.continueActivate();
		delete this.continueActivate;
		Mojo.Log.error("this.continueActivate()");
	}
};
MainAssistant.prototype.initDicts = function() {
	Mojo.Log.error("MainAssistant.prototype.initDicts");
	this.dicts = [];
	this.pendingLoads = 2;
	queryDicts(this.queryDictsComplete.bind(this));
	setTimeout(function() { queryDictsC(this.queryDictsComplete.bind(this)); }.bind(this), 0);
};
