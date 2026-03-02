/* Copyright (C) 2007 gnombat@users.sourceforge.net */
/* License: http://shjs.sourceforge.net/doc/license.html */

function sh_highlightString(inputString,language,builder){var patternStack={_stack:[],getLength:function(){return this._stack.length;},getTop:function(){var stack=this._stack;var length=stack.length;if(length===0){return undefined;}
return stack[length-1];},push:function(state){this._stack.push(state);},pop:function(){if(this._stack.length===0){throw"pop on empty stack";}
this._stack.pop();}};var pos=0;var currentStyle=undefined;var output=function(s,style){var length=s.length;if(length===0){return;}
if(!style){var pattern=patternStack.getTop();if(pattern!==undefined&&!('state'in pattern)){style=pattern.style;}}
if(currentStyle!==style){if(currentStyle){builder.endElement();}
if(style){builder.startElement(style);}}
builder.text(s);pos+=length;currentStyle=style;};var endOfLinePattern=/\r\n|\r|\n/g;endOfLinePattern.lastIndex=0;var inputStringLength=inputString.length;while(pos<inputStringLength){var start=pos;var end;var startOfNextLine;var endOfLineMatch=endOfLinePattern.exec(inputString);if(endOfLineMatch===null){end=inputStringLength;startOfNextLine=inputStringLength;}
else{end=endOfLineMatch.index;startOfNextLine=endOfLinePattern.lastIndex;}
var line=inputString.substring(start,end);var matchCache=null;var matchCacheState=-1;for(;;){var posWithinLine=pos-start;var pattern=patternStack.getTop();var stateIndex=pattern===undefined?0:pattern.next;var state=language[stateIndex];var numPatterns=state.length;if(stateIndex!==matchCacheState){matchCache=[];}
var bestMatch=null;var bestMatchIndex=-1;for(var i=0;i<numPatterns;i++){var match;if(stateIndex===matchCacheState&&(matchCache[i]===null||posWithinLine<=matchCache[i].index)){match=matchCache[i];}
else{var regex=state[i].regex;regex.lastIndex=posWithinLine;match=regex.exec(line);matchCache[i]=match;}
if(match!==null&&(bestMatch===null||match.index<bestMatch.index)){bestMatch=match;bestMatchIndex=i;}}
matchCacheState=stateIndex;if(bestMatch===null){output(line.substring(posWithinLine),null);break;}
else{if(bestMatch.index>posWithinLine){output(line.substring(posWithinLine,bestMatch.index),null);}
pattern=state[bestMatchIndex];var newStyle=pattern.style;var matchedString;if(newStyle instanceof Array){for(var subexpression=0;subexpression<newStyle.length;subexpression++){matchedString=bestMatch[subexpression+1];output(matchedString,newStyle[subexpression]);}}
else{matchedString=bestMatch[0];output(matchedString,newStyle);}
if('next'in pattern){patternStack.push(pattern);}
else{if('exit'in pattern){patternStack.pop();}
if('exitall'in pattern){while(patternStack.getLength()>0){patternStack.pop();}}}}}
if(currentStyle){builder.endElement();}
currentStyle=undefined;if(endOfLineMatch){builder.text(endOfLineMatch[0]);}
pos=startOfNextLine;}}
function sh_getClasses(element){var result=[];var htmlClass=element.className;if(htmlClass&&htmlClass.length>0){var htmlClasses=htmlClass.split(" ");for(var i=0;i<htmlClasses.length;i++){if(htmlClasses[i].length>0){result.push(htmlClasses[i]);}}}
return result;}
function sh_addClass(element,name){var htmlClasses=sh_getClasses(element);for(var i=0;i<htmlClasses.length;i++){if(name.toLowerCase()===htmlClasses[i].toLowerCase()){return;}}
htmlClasses.push(name);element.className=htmlClasses.join(" ");}
function sh_getText(element){if(element.nodeType===3||element.nodeType===4){return element.data;}
else if(element.childNodes.length===1){return sh_getText(element.firstChild);}
else{var result='';for(var i=0;i<element.childNodes.length;i++){result+=sh_getText(element.childNodes.item(i));}
return result;}}
function sh_isEmailAddress(url){if(/^mailto:/.test(url)){return false;}
return url.indexOf('@')!==-1;}
var sh_builder={init:function(htmlDocument,element){while(element.hasChildNodes()){element.removeChild(element.firstChild);}
this._document=htmlDocument;this._element=element;this._currentText=null;this._documentFragment=htmlDocument.createDocumentFragment();this._currentParent=this._documentFragment;this._span=htmlDocument.createElement("span");this._a=htmlDocument.createElement("a");},startElement:function(style){if(this._currentText!==null){this._currentParent.appendChild(this._document.createTextNode(this._currentText));this._currentText=null;}
var span=this._span.cloneNode(true);span.className=style;this._currentParent.appendChild(span);this._currentParent=span;},endElement:function(){if(this._currentText!==null){if(this._currentParent.className==='sh_url'){var a=this._a.cloneNode(true);a.className='sh_url';var url=this._currentText;if(url.length>0&&url.charAt(0)==='<'&&url.charAt(url.length-1)==='>'){url=url.substr(1,url.length-2);}
if(sh_isEmailAddress(url)){url='mailto:'+url;}
a.setAttribute('href',url);a.appendChild(this._document.createTextNode(this._currentText));this._currentParent.appendChild(a);}
else{this._currentParent.appendChild(this._document.createTextNode(this._currentText));}
this._currentText=null;}
this._currentParent=this._currentParent.parentNode;},text:function(s){if(this._currentText===null){this._currentText=s;}
else{this._currentText+=s;}},close:function(){if(this._currentText!==null){this._currentParent.appendChild(this._document.createTextNode(this._currentText));this._currentText=null;}
this._element.appendChild(this._documentFragment);}};function sh_highlightElement(htmlDocument,element,language){sh_addClass(element,"sh_sourceCode");var inputString;if(element.childNodes.length===0){return;}
else{inputString=sh_getText(element);}
sh_builder.init(htmlDocument,element);sh_highlightString(inputString,language,sh_builder);sh_builder.close();}
function sh_highlightHTMLDocument(htmlDocument){if(!window.sh_languages){return;}
var nodeList=htmlDocument.getElementsByTagName("pre");for(var i=0;i<nodeList.length;i++){var element=nodeList.item(i);var htmlClasses=sh_getClasses(element);for(var j=0;j<htmlClasses.length;j++){var htmlClass=htmlClasses[j].toLowerCase();if(htmlClass==="sh_sourcecode"){continue;}
var prefix=htmlClass.substr(0,3);if(prefix==="sh_"){var language=htmlClass.substring(3);if(language in sh_languages){sh_highlightElement(htmlDocument,element,sh_languages[language]);}
else{throw"Found <pre> element with class='"+htmlClass+"', but no such language exists";}}}}}
function sh_highlightDocument(){sh_highlightHTMLDocument(document);}