var FmtUtil = {
	LfToBr : function(text) {
		return text? text.replace(/\\n/g, "<br />") : null;
	},
	texToHtmlMarks : function(text, marks) {
		for(var i = 0; i < marks.length; ++i) {
			text = text? text.replace(RegExp("\\\\"+marks[i]+"\\{(.+?)\\}", "g"), '<div class="'+marks[i]+'">$1</div>') : null;
		}
		return text;
	},
	texToHtml : function(text) {
		return text? text.replace(/\\([a-zA-Z_]+?)\{([^{}]+?)\}/g, '<div class="$1">$2</div>') : null;
	}
};
var _dumpObj = function(obj) {
	Mojo.Log.info("#######################obj:", obj? obj.toString() : "<undefined>");
	for(var key in obj)
	{
		Mojo.Log.info("####obj:", key, obj[key]);
	}
};
