/* DictionaryC {
		db,
		name,
		count,
		meta { name, version, info, template, formatters, styles, ... },
		rowid
	}
			
   model { dictIndex, word, }
*/

function DictionaryC(dictInfo) {
	this.filename = dictInfo.filename;
	this.name = dictInfo.name;
	this.count = dictInfo.count;
/*	this.db = undefined;
 	this.meta = {};
	this.rowid = 0;*/
}
///////////
DictionaryC.prototype.free = function() {
	Mojo.Log.error("+C.free");
	var plugin = DictionaryC.prototype.plugin;
	plugin.closeDict(this.db);
	delete this.db;
	
	/*plugin.onGetMeta = undefined;
	plugin.onLookUp = undefined;
	plugin.onLookUpById = undefined;*/
	Mojo.Log.error("-C.free");
};
DictionaryC.prototype.load = function(callback) {
	this.loadCallback = callback;

	Mojo.Log.error("+C.load");
	var plugin = DictionaryC.prototype.plugin;
	
	plugin.onGetMeta = this.onGetMeta.bind(this);
	plugin.onLookUp = this.onLookUp.bind(this);
	plugin.onLookUpById = this.onLookUp.bind(this);
	
	this.db = plugin.openDict(this.filename);
	Mojo.Log.error("+C.getMeta", plugin.onGetMeta);
	plugin.getMeta(this.db);
	Mojo.Log.error("-C.getMeta", plugin.onGetMeta);
	Mojo.Log.error("-C.load");
};

DictionaryC.prototype.onGetMeta = function(jMeta) {
	// update dict meta
	Mojo.Log.error("+C.onGetMeta");
	this.meta = eval('('+jMeta+')');
	setTimeout(function() {
		Mojo.Log.error("+C.loadCallback", this.loadCallback);
		this.loadCallback(this.meta); 
		delete this.loadCallback;
		Mojo.Log.error("-C.loadCallback");
	}.bind(this), 0);
	Mojo.Log.error("-C.onGetMeta");
};

//////////
DictionaryC.prototype.onLookUp = function(jRow) {
	var row = eval('('+jRow+')');
	setTimeout(function() {
		if(row) {
			this.rowid = row.rowid;
			this.lookUpCallback(row);
		} else {
			this.lookUpCallback();
		}
	}.bind(this), 0);
};
DictionaryC.prototype.lookUp = function(word, callback, caseSense) {
	this.lookUpCallback = callback;
	
	var plugin = DictionaryC.prototype.plugin;
	plugin.lookUp(this.db, word, caseSense ? 1 : 0);
};
DictionaryC.prototype.lookNext = function(offset, callback) {
	this.lookUpCallback = callback;
	this.rowid += offset;
	if(this.rowid < 1) { this.rowid = 1; }
	else if(this.rowid > this.count) { this.rowid = this.count; }
	
	var plugin = DictionaryC.prototype.plugin;
	plugin.lookUpById(this.db, this.rowid);
};


var queryDictsC = function(callback) {
	Mojo.Log.error("queryDictsC");
	DictionaryC.prototype.plugin = $("plugin");
	var plugin = DictionaryC.prototype.plugin;

	var queryData = { dicts: [], queryDictsCallback: callback };

	plugin.onQueryDicts = function(jDicts) {
		var dicts = eval('('+jDicts+')');
		for(var i = 0; i < dicts.length; ++i) {
			this.dicts.push(new DictionaryC(dicts[i]));
		}
		setTimeout(function() {
			Mojo.Log.error("queryDictsC-callback");
			this.queryDictsCallback(this.dicts); 
			delete this.queryDictsCallback;
		}.bind(this), 0);
	}.bind(queryData);

	plugin.queryDicts("/media/internal/jdict");
};
