/* Dictionary {
		db,
		name,
		count,
		meta { name, version, info, template, formatters, styles, ... },
		renderParams { object, meta.template, meta.formatters },
		rowid
	}
			
   model { dictIndex, word, }
*/

function Dictionary(name) {
	this.db = openDatabase("ext:"+name, 1);
/*	this.name = null;
	this.count = 0;
	this.meta = {};
	this.rowid = 0;*/
}
Dictionary.prototype.init = function(callback) {
	this.initCallback = callback;
	var sql = "select * from (select value as name from meta where key='name') left join (select max(rowid) as count from dict)";
	this.db.transaction((function(transaction) {
		transaction.executeSql(sql, [],
							   this.handleInitSuccess.bind(this),
							   this.handleInitError.bind(this));
	}).bind(this));
};
Dictionary.prototype.handleInitSuccess = function(transaction, SQLResultSet) {
	if(SQLResultSet.rows.length > 0) {
		var item = SQLResultSet.rows.item(0);
		this.name = item.name;
		this.count = item.count;
	} else {
		this.db = null;
	}
	this.initCallback(this);
	delete this.initCallback;
};
Dictionary.prototype.handleInitError = function(dict, transaction, error) {
	this.db = null;
	this.initCallback();
	delete this.initCallback;
};
///////////
Dictionary.prototype.release = function() {

};
Dictionary.prototype.load = function(callback) {
	this.loadCallback = callback;
	// dict meta and render parameters
	var template = '<div class="word">#{-word}</div><div class="pron">#{-pron}</div><div class="expl">#{-expl}</div><div class="sent">#{-sent}</div>';
	this.meta = { template : template };
	this.renderParams = { };
	this.db.transaction((function(transaction) {
			transaction.executeSql("select * from meta", [],
								this.handleLoadSuccess.bind(this),
								this.handleLoadError.bind(this));
		}).bind(this));
};

Dictionary.prototype.handleLoadSuccess = function(transaction, SQLResultSet) {
	// update dict meta
	for(var i = 0; i < SQLResultSet.rows.length; ++i) {
		var item = SQLResultSet.rows.item(i);
		this.meta[item.key] = item.value;
	}
	// setup render params
	this.renderParams = { inline: this.meta.template,
		formatters: this.meta.formatters? eval('('+this.meta.formatters+')') : undefined
	};
	
	this.loadCallback(this.meta);
	delete this.loadCallback;
};
Dictionary.prototype.handleLoadError = function(transaction, error) {
	Mojo.Log.error("sql error: ", error.message);
	this.loadCallback();
	delete this.loadCallback;
};
//////////
Dictionary.prototype.lookUp = function(word, callback) {
	this.lookUpCallback = callback;
	
	var keyword = word.replace(/\'/g, "''");
	var caseSense = Model.model.caseSensitive ? "" : " collate nocase";
	var sqlQuery = "Select rowid,* from dict where word>=" + "'"+keyword+"'" + caseSense +" limit 1";
	this.db.transaction((function(transaction) {
			transaction.executeSql(sqlQuery, [],
								   this.handleLookUpSucces.bind(this),
								   this.handleLookUpError.bind(this));
		}).bind(this));	
};
Dictionary.prototype.lookNext = function(offset, callback) {
	this.lookUpCallback = callback;
	
	this.rowid = parseInt(this.rowid, 10) + offset;
	if(this.rowid < 1) { this.rowid = 1; }
	if(this.rowid > this.count) { this.rowid = this.count; }
	var sqlQuery = "Select rowid,* from dict where rowid=" + this.rowid;
	this.db.transaction((function(transaction) {
			transaction.executeSql(sqlQuery, [],
								   this.handleLookUpSucces.bind(this),
								   this.handleLookUpError.bind(this));
		}).bind(this));	
};
Dictionary.prototype.handleLookUpSucces = function(transaction, SQLResultSet) {
	if(SQLResultSet.rows.length > 0) {
		this.renderParams.object = SQLResultSet.rows.item(0);
		this.rowid = SQLResultSet.rows.item(0).rowid;
		this.lookUpCallback(this.renderParams);
		this.renderParams.object = null;
	}
};
Dictionary.prototype.handleLookUpError = function(transaction, error) {
	Mojo.Log.error("sql error: ", error.message);
	this.lookUpCallback();
};

