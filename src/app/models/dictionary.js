/* Dictionary {
		db,
		name,
		count,
		meta { name, version, info, template, formatters, styles, ... },
		rowid
	}
			
   model { dictIndex, word, }
*/

function Dictionary(filename) {
	this.filename = "ext:"+filename;
/*	this.db = undefined;
	this.name = undefined;
	this.count = 0;
	this.meta = {};
	this.rowid = 0;*/
}
Dictionary.prototype.peek = function(callback) {
	this.peekCallback = callback;
	this.db = openDatabase(this.filename, 1);
	var sql = "select * from (select value as name from meta where key='name') left join (select max(rowid) as count from dict)";
	this.db.readTransaction((function(transaction) {
		transaction.executeSql(sql, [],
							   this.handlePeekSuccess.bind(this),
							   this.handlePeekError.bind(this));
	}).bind(this));
};
Dictionary.prototype.handlePeekSuccess = function(transaction, SQLResultSet) {
	if(SQLResultSet.rows.length > 0) {
		var item = SQLResultSet.rows.item(0);
		this.name = item.name;
		this.count = item.count;
		this.peekCallback(this);
	} else {
		this.peekCallback();
	}
	delete this.peekCallback;
};
Dictionary.prototype.handlePeekError = function(dict, transaction, error) {
	this.db = undefined;
	this.peekCallback();
	delete this.peekCallback;
};
///////////
Dictionary.prototype.free = function() {
	delete this.db;
};
Dictionary.prototype.load = function(callback) {
	this.loadCallback = callback;
	// dict meta and render parameters
	var template = '<div class="word">#{-word}</div><div class="expl">#{-expl}</div>';
	this.meta = { template : template };
	this.db = this.db || openDatabase(this.filename, 1);
	this.db.readTransaction((function(transaction) {
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
	this.loadCallback(this.meta);
	delete this.loadCallback;
};
Dictionary.prototype.handleLoadError = function(transaction, error) {
	Mojo.Log.error("sql error: ", error.message);
	this.loadCallback();
	delete this.loadCallback;
};
//////////
Dictionary.prototype.lookUp = function(word, callback, caseSense) {
	this.lookUpCallback = callback;
	
	var keyword = word.replace(/\'/g, "''");
	var caseCollate = caseSense ? "" : " collate nocase";
	var sqlQuery = "Select rowid,* from dict where word>=" + "'"+keyword+"'" + caseCollate +" limit 1";
	this.db.readTransaction((function(transaction) {
			transaction.executeSql(sqlQuery, [],
								   this.handleLookUpSucces.bind(this),
								   this.handleLookUpError.bind(this));
		}).bind(this));	
};
Dictionary.prototype.lookNext = function(offset, callback) {
	this.lookUpCallback = callback;
	
	this.rowid = parseInt(this.rowid, 10) + offset;
	if(this.rowid < 1) { this.rowid = 1; }
	else if(this.rowid > this.count) { this.rowid = this.count; }
	var sqlQuery = "Select rowid,* from dict where rowid=" + this.rowid;
	this.db.readTransaction((function(transaction) {
			transaction.executeSql(sqlQuery, [],
								   this.handleLookUpSucces.bind(this),
								   this.handleLookUpError.bind(this));
		}).bind(this));	
};
Dictionary.prototype.handleLookUpSucces = function(transaction, SQLResultSet) {
	if(SQLResultSet.rows.length > 0) {
		this.rowid = SQLResultSet.rows.item(0).rowid;
		this.lookUpCallback(SQLResultSet.rows.item(0));
	} else {
		this.lookUpCallback();
	}
};
Dictionary.prototype.handleLookUpError = function(transaction, error) {
	Mojo.Log.error("sql error: ", error.message);
	this.lookUpCallback();
};


var handlePeek = function(dict) {
	// wait untill all dicts inited
	if(dict) {
		this.dicts.push(dict);
	}
	
	--this.pendingInits;
	if(this.pendingInits > 0) { return; }
	
	// all dicts inited
	delete this.pendingInits;

	this.queryDictsCallback(this.dicts);
};
var queryDicts = function(callback) {
	var queryData = { dicts: [], pendingInits: 7, queryDictsCallback: callback };
	
	for(var i = 0; i < queryData.pendingInits; ++i) {
		var dict = new Dictionary("dict"+i);
		dict.peek(handlePeek.bind(queryData));
	}
};
