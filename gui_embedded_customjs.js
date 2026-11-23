function toggleClientMenu()
{
	var m = document.getElementById('clientsub');
	if (!m) return;
	m.style.display = (m.style.display === 'block') ? 'none' : 'block';
}
function imgrequest( url, el )
{
	var httpRequest;
	try { httpRequest = new XMLHttpRequest(); }
	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }
	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }
	if ( typeof(el)!='undefined' ) {
		el.style.opacity = '0.7';
		httpRequest.onreadystatechange = function()
		{
			if (httpRequest.readyState == 4) el.style.opacity = '1.0';
		}
	}
	httpRequest.open('GET', url, true);
	httpRequest.send(null);
}
function btnrequest( url, sid )
{
	var s = document.getElementById(sid);
	if (s) s.innerHTML = '<span class=busy>Running...</span>';
	var x = new XMLHttpRequest();
	x.open('GET', url, true);
	x.onreadystatechange = function()
	{
		if (x.readyState == 4) {
			if (x.status == 200) { if (s) s.innerHTML = x.responseText; }
			else if (s) s.innerHTML = '<span class=failed>HTTP error ' + x.status + '</span>';
		}
	};
	x.send(null);
}
function sortTable(el,n){var t=el.closest?el.closest('table'):null;if(!t)return;var tb=t.tBodies[0]||t;var r=t.tHead?t.tHead.rows[0].cells:t.rows[0].cells;var h=r[n];if(!h)return;var b=Array.from(tb.rows);var a=window['sortAsc']=(window['sortCol']==n)?-window['sortAsc']:1;window['sortCol']=n;b.sort(function(x,y){var p=x.cells[n].textContent.trim(),q=y.cells[n].textContent.trim();var pn=parseFloat(p),qn=parseFloat(q);return isNaN(pn)||isNaN(qn)?(p>q?1:p<q?-1:0)*a:(pn-qn)*a});b.forEach(function(r){tb.appendChild(r)});}
function bindSortable(){var ts=document.querySelectorAll('table.maintable');for(var k=0;k<ts.length;k++){(function(t){var ths=t.querySelectorAll('th');for(var i=0;i<ths.length;i++){(function(n){ths[n].style.cursor='pointer';ths[n].title='Click to sort';ths[n].onclick=function(){sortTable(this,n);};})(i);}})(ts[k]);}}
function fetchDebugLog(){if(window.dbgPaused)return;var d=document.getElementById('dbglog');if(!d)return;var x=new XMLHttpRequest();x.open('GET','/debug?action=log',true);x.onreadystatechange=function(){if(x.readyState==4&&x.status==200){d.innerHTML=x.responseText;}};x.send(null);}
function dbgSyncBtn(){var b=document.getElementById('dbgpause');var t=document.getElementById('dbgstate');var p=!!window.dbgPaused;if(b){b.textContent=p?'Resume live log':'Stop live log';b.setAttribute('aria-pressed',p?'true':'false');}if(t)t.textContent=p?'Paused - log frozen, other sections still refresh':'Live';}
function toggleDbgPause(){window.dbgPaused=!window.dbgPaused;dbgSyncBtn();if(!window.dbgPaused)fetchDebugLog();}
function setDebugFilter(v){imgrequest('/debug?action=debug&value='+v);window.dbgPaused=false;dbgSyncBtn();setTimeout(fetchDebugLog,150);}
function toggleDbgRow(id,url,feed){var r=document.getElementById('dbgrow_'+id);if(r){r.parentNode.removeChild(r);window.dbgOpen=null;return;}var tr=document.getElementById('Row'+id);if(!tr)return;window.dbgOpen={id:id,url:url,feed:feed||''};var x=new XMLHttpRequest();x.open('GET',url,true);x.onreadystatechange=function(){if(x.readyState==4&&x.status==200){var t=document.createElement('tr');t.id='dbgrow_'+id;t.className='dbgrow';var c=document.createElement('td');c.colSpan=99;c.innerHTML=x.responseText;t.appendChild(c);var old=document.getElementById('dbgrow_'+id);if(old)old.parentNode.removeChild(old);tr.parentNode.insertBefore(t,tr.nextSibling);if(window.dbgOpen&&window.dbgOpen.id==id&&window.dbgOpen.feed){var f=document.createElement('div');f.id='cwfeedbox_'+id;f.className='cwfeedbox';c.appendChild(f);cwfeedPoll(id,window.dbgOpen.feed,f);}}};x.send(null);}
function cwfeedPoll(id,url,el){(function tick(){var d=document.getElementById('dbgrow_'+id);if(!d){setTimeout(tick,2000);return;}var x=new XMLHttpRequest();x.open('GET',url,true);x.onreadystatechange=function(){if(x.readyState==4){if(x.status==200){if(el)el.innerHTML=x.responseText;}setTimeout(tick,2000);}};x.send(null);})();}
function reopenDbg(){if(!window.dbgOpen)return;var o=window.dbgOpen;var tr=document.getElementById('Row'+o.id);var d=document.getElementById('dbgrow_'+o.id);if(tr&&!d)toggleDbgRow(o.id,o.url,o.feed);}
function colorStatusRows(){var tds=document.querySelectorAll('table.maintable td.offline, table.maintable td.online, table.maintable td.busy');for(var i=0;i<tds.length;i++){var tr=tds[i].parentNode;if(!tr)continue;tr.classList.remove('row-offline');tr.classList.remove('row-online');tr.classList.remove('row-busy');if(tds[i].classList.contains('offline'))tr.classList.add('row-offline');else if(tds[i].classList.contains('online'))tr.classList.add('row-online');else tr.classList.add('row-busy');}}
setInterval(colorStatusRows,2000);
function applyTheme(t){if(t==='light'){document.body.classList.add('light-mode');document.documentElement.classList.add('light-mode');}else{document.body.classList.remove('light-mode');document.documentElement.classList.remove('light-mode');}var btn=document.getElementById('themeToggle');if(btn){btn.textContent=(t==='light')?'Dark theme':'Light theme';}}
function toggleTheme(){var cur=(document.body.classList.contains('light-mode'))?'light':'dark';var t=(cur==='light')?'dark':'light';applyTheme(t);try{localStorage.setItem('theme',t);}catch(e){}}
document.addEventListener('DOMContentLoaded',function(){var t='dark';try{t=localStorage.getItem('theme')||'dark';}catch(e){}try{var q=new URLSearchParams(location.search).get('theme');if(q==='light'||q==='dark')t=q;}catch(e){}applyTheme(t);bindSortable();colorStatusRows();if(document.getElementById('dbglog'))setInterval(fetchDebugLog,2000);var d=document.getElementById('mainDiv');if(d){var f=document.createElement('div');f.className='home-footer';var bv=document.querySelector('.bt-ver');var vm=(bv?bv.textContent:document.title).match(/v([0-9.]+)/);f.innerHTML="<span class='hf-ver'>MultiCS r1000 v"+(vm?vm[1]:'')+" - All Rights Reserved - Sharillas@2026</span>";if(d.parentNode){d.parentNode.insertBefore(f,d.nextSibling);}}});
var leTrigger=null;
function leOpen(type,id){
 leTrigger=document.activeElement;
 var x=new XMLHttpRequest();
 x.open('GET','/editline?action=form&type='+type+'&id='+id,true);
 x.onreadystatechange=function(){
  if(x.readyState==4&&x.status==200){
   var old=document.getElementById('lemodal');if(old)old.parentNode.removeChild(old);
   var d=document.createElement('div');d.id='lemodal';
   d.setAttribute('role','dialog');d.setAttribute('aria-modal','true');d.setAttribute('aria-label','Edit line');
   d.innerHTML=x.responseText;
   document.body.appendChild(d);
   var lp=document.getElementById('le_proto');
   if(lp&&lp.tagName==='SELECT'){lp.onchange=function(){var k=document.getElementById('lerow_key');if(k)k.style.display=(lp.value=='newcamd')?'':'none';};}
   var f=d.querySelector('input:not([type=hidden]),select,button');if(f)f.focus();
  }
 };
 x.send(null);
}
function leClose(){var d=document.getElementById('lemodal');if(d)d.parentNode.removeChild(d);if(leTrigger&&leTrigger.focus){try{leTrigger.focus();}catch(e){}}leTrigger=null;}
document.addEventListener('keydown',function(e){if(e.key==='Escape'&&document.getElementById('lemodal'))leClose();});
function leSave(){
 var g=function(n){var e=document.getElementById('le_'+n);return e?e.value:'';};
 var b64=function(s){try{return btoa(unescape(encodeURIComponent(s)));}catch(e){return '';}};
 var bl=[];document.querySelectorAll('#lemodal .le-bool:checked').forEach(function(c){if(c.dataset&&c.dataset.key)bl.push(c.dataset.key);});
 var fb=document.getElementById('le_fblock0');
 var en=[];document.querySelectorAll('#lemodal .le-enum').forEach(function(c){if(c.dataset&&c.dataset.key)en.push(c.dataset.key+':'+c.value);});
 var f=function(k,v){return k+'='+encodeURIComponent(v);};
 var body=[f('old',b64(g('old'))),f('proto',g('proto')),f('host',b64(g('host'))),f('port',g('port')),f('user',b64(g('user'))),f('pass',b64(g('pass'))),f('key',b64(g('key'))),f('opts',b64(g('opts'))),f('bool',bl.join(',')),f('enum',en.join(','))];
 if(fb)body.push(f('fblock0',fb.checked?1:0));
 body.push('end=1');
 var x=new XMLHttpRequest();x.open('POST','/editline?action=save&type='+encodeURIComponent(g('type'))+'&id='+encodeURIComponent(g('id')),true);
 x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
 x.onreadystatechange=function(){if(x.readyState!=4)return;var t=x.responseText||'';if(x.status==200&&t.indexOf("class='miss'")<0){leClose();if(typeof updateDiv==='function')setTimeout(updateDiv,800);return;}var d=document.getElementById('lemodal');if(d){var o=document.getElementById('le_err');if(o)o.parentNode.removeChild(o);var s=document.createElement('div');s.id='le_err';s.innerHTML=t||'ERROR';d.appendChild(s);}};
 x.send(body.join('&'));
}
