var MSG_COPY_MAGNET = '%[WebserverMagnetCopied]';
var MSG_CONFIRM_REMOVE = '%[ReallyRemove]';
var MSG_YES = '%[Yes]';
var MSG_NO = '%[No]';
var MSG_OK = '%[Ok]';

var infotipList = [];

function removeInfotip(pos)
{
	if (infotipList[pos].t) clearTimeout(infotipList[pos].t);
	var elem = document.getElementById(infotipList[pos].id);
	if (elem && elem.parentNode) elem.parentNode.removeChild(elem);
	infotipList.splice(pos, 1);
}

function hideInfotip(id)
{
	for (var i = 0; i < infotipList.length; i++)
		if (infotipList[i].id === id)
		{
			removeInfotip(i);
			break;
		}
}

function showInfotip(id, text, success)
{
	var elem = document.getElementById(id);
	if (!elem) return;

	for (var i = 0; i < infotipList.length; i++)
		if (infotipList[i].contentId === id)
		{
			removeInfotip(i);
			break;
		}
 
	var tipId = id + '-infotip';
	var info = { id: tipId, contentId: id, t: setTimeout(hideInfotip, 5000, tipId) };
	infotipList.push(info);

	var it = document.createElement('div');
	it.innerHTML = text;
	it.className = success ? 'infotip' : 'infotip-failure';
	it.id = tipId;
	elem.appendChild(it);
}

function copyMagnet(rowId)
{
	var row = document.getElementById(rowId);
	if (!row) return;
	var magnet = row.getAttribute('data-magnet');
	if (!magnet) return;
	if (copyTextToClipboard(magnet))
		showInfotip(rowId + '-magnet', MSG_COPY_MAGNET, true);
}

function copyTextToClipboardLegacy(text)
{
	var textArea = document.createElement("textarea");
	textArea.value = text;
	textArea.style.top = '0';
	textArea.style.left = '0';
	textArea.style.position = 'fixed';

	document.body.appendChild(textArea);
	textArea.focus();
	textArea.select();

	var result = document.execCommand('copy');	
	document.body.removeChild(textArea);
	return result;
}

function copyTextToClipboard(text)
{
	if (navigator.clipboard)
	{
		navigator.clipboard.writeText(text);
		return true;
	}
	return copyTextToClipboardLegacy(text);
}

function sortTable(id)
{
	var link = document.getElementById(id);
	if (link) document.location = link.getAttribute('href');
}

function setElemText(elem, text)
{
	if ('textContent' in elem)
		elem.textContent = text;
	else if ('innerText' in el)
		elem.innerText = text;
	else
		elem.innerHTML = text;
}

function dismissPopup()
{
	var bg = document.getElementById('modal-background');
	if (bg) bg.parentNode.removeChild(bg);
	var box = document.getElementById('modal-box');
	if (box) box.parentNode.removeChild(box);
}

function createDiv(className, parent)
{
	var d = document.createElement('div');
	d.className = className;
	if (parent) parent.appendChild(d);
	return d;
}

function showPopup(text, buttons, icon)
{
	var bg = document.getElementById('modal-background');
	if (!bg)
	{
		bg = document.createElement('div');
		bg.id = 'modal-background';
		bg.className = 'modal-background';
		document.body.appendChild(bg);
		setTimeout(function(el) { el.className += ' active-background'; }, 1, bg);
	}

	var div1 = createDiv('modal-box-container', null);
	var div2 = createDiv('modal-box', div1);
	div2.id = 'modal-box';
	var div3 = createDiv('modal-box-row-container', div2);
	var div4 = createDiv('modal-box-row', div3);
	var div5 = createDiv('img', div4);
	var div6 = createDiv('text', div4);
	setElemText(div6, text);

	if (icon)
	{
		var img = document.createElement('img');
		img.src = icon;
		div5.appendChild(img);
	}

	var boxButtons = createDiv('modal-box-buttons', div2);
	for (var i = 0; i < buttons.length; ++i)
	{
		var input = document.createElement('input');
		var onclick = buttons[i].onclick;
		input.type = 'button';
		input.value = buttons[i].text;
		input.onclick = onclick ? onclick : dismissPopup;
		boxButtons.appendChild(input);
	}
	document.body.appendChild(div1);
	setTimeout(function(el) { el.className += ' active-box'; }, 1, div1);
}

function preventDefault(e)
{
	if (e.preventDefault)
		e.preventDefault();
	else
		e.returnValue = false;
}

function getFormData(form)
{
	var str = '';
	var submitUrl = null;
	for (var i = 0; i < form.elements.length; i++)
	{
		var elem = form.elements[i];
		if (!elem.name) continue;
		if (elem.nodeName === 'INPUT' && elem.type)
		{
			var type = elem.type.toLowerCase();
			if (type === 'text'  || type === 'password' || type === 'hidden' || (type === 'checkbox' && elem.checked))
			{
				if (str.length) str += '&';
				str += elem.name + '=';
				str += encodeURIComponent(elem.value);
			}
		}
		else if (elem.nodeName === 'TEXTAREA' || elem.nodeName === 'SELECT')
		{
			if (str.length) str += '&';
			str += elem.name + '=';
			str += encodeURIComponent(elem.value);
		}
	}
	return str;
}

function parseJson(s)
{
	var j = null;
	if (JSON && JSON.parse)
	{
		try { j = JSON.parse(s); }
		catch (e) {}
	}
	else
		j = eval('(' + s + ')');
	return j;
}

function sendForm(form)
{
	var submitId = '';
	for (var i = 0; i < form.elements.length; i++)
	{
		var elem = form.elements[i];
		if (elem.nodeName === 'INPUT' && elem.type)
		{
			var type = elem.type.toLowerCase();
			if (type === 'submit')
			{
				submitId = elem.parentNode.id;
				break;
			}
		}
	}
	var data = getFormData(form);
	if (data.length) data += '&';
	data += 'json=1';
	sendRequest('POST', form.action, data, function(obj)
	{
		if (obj.status != 200)
		{
			if (obj.status >= 300) showInfotip(submitId, obj.status + ' ' + obj.statusText, false);
			return;
		}
		var j = parseJson(obj.responseText);
		if (j.redirect) document.location = j.redirect;
		else if (j.message && submitId) showInfotip(submitId, j.message, j.success);
	});
}

function sendAction(link)
{
	dismissPopup();
	document.location = link;
}

function performButtonAction(buttonId)
{
	preventDefault(window.event);
	var elem = document.getElementById(buttonId);
	if (!elem) return;
	var url = elem.getAttribute('data-action-url');
	if (url) document.location = url;
}

function sendRequest(method, url, postData, callback)
{
	var obj;
	try { obj = new XMLHttpRequest(); }
	catch (e) {}
	if (!obj)
	{
		alert("Can't initialize XMLHttpRequest");
		return;
	}

	obj.open(method, url, true);
	obj.onreadystatechange = function()
	{
		if (obj.readyState != 4) return;
		callback(obj);
	};
	obj.setRequestHeader('Accept', "application/json");
	obj.send(postData);
}

function removeItem(rowId)
{
	preventDefault(window.event);
	var link = document.getElementById(rowId + '-remove');
	if (!link) return;
	var removeAction = link.getAttribute('href');
	showPopup(MSG_CONFIRM_REMOVE, [{ text: MSG_YES, onclick: function() {sendAction(link);} }, { text: MSG_NO }], 'question32.png');
}

function sendSearch()
{
	preventDefault(window.event);
	var form = document.getElementById('search-form');
	if (!form) return;
	sendForm(form);
	var elem = document.getElementById('search-string');
	if (elem) elem.value = '';
}

function performRowAction(rowId, action)
{
	var targetId = rowId + '-' + action;
	var link = document.getElementById(targetId);
	if (!link) return;
	var url = link.getAttribute('href');
	url += '&json=1';
	sendRequest('GET', url, null, function(obj)
	{
		if (obj.status != 200)
		{
			if (obj.status >= 300) showInfotip(targetId, obj.status + ' ' + obj.statusText, false);
			return;
		}
		var j = parseJson(obj.responseText);
		if (j.message && j.rowId) showInfotip(targetId, j.message, j.success);
	});
}

function addToQueue(rowId)
{
	preventDefault(window.event);
	performRowAction(rowId, 'download');
}

function grantSlot(rowId)
{
	preventDefault(window.event);
	performRowAction(rowId, 'grant');
}

function addMagnet()
{
	preventDefault(window.event);
	var form = document.getElementById('add-magnet-form');
	if (!form) return;
	sendForm(form);
	var elem = document.getElementById('magnet-string');
	if (elem) elem.value = '';
}

function refreshShare()
{
	preventDefault(window.event);
	var form = document.getElementById('refresh-share-form');
	if (form) sendForm(form);
}
