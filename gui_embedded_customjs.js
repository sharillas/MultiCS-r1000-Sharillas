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
/* v1.70.16 - polling estilo OSCam: um pedido de cada vez, actualiza no sitio (sem trocar o HTML todo),
   pausa e -/+ com intervalo guardado por separador, indicador POLLERR e batimento a cada poll */
var mcsP={url:'',iv:5000,paused:false,timer:null,busy:false,again:false,inited:false,xhr:null};
function mcsSS(k,v){try{if(v===undefined)return sessionStorage.getItem(k);sessionStorage.setItem(k,v);}catch(e){}return null;}
function mcsPollUI(){var s=Math.round(mcsP.iv/1000),v=document.getElementById('pl_val'),b=document.getElementById('pl_pause'),a=document.getElementById('ar_val'),d=document.getElementById('pl_dec'),i=document.getElementById('pl_inc');if(v)v.value=mcsP.paused?'--':s;if(b){b.textContent=mcsP.paused?'Resume polling':'Pause polling';b.setAttribute('aria-pressed',mcsP.paused?'true':'false');}if(a)a.textContent=mcsP.paused?'Paused':'Live '+s+'s';if(d)d.disabled=mcsP.paused||s<=1;if(i)i.disabled=mcsP.paused||s>=99;}
function mcsPollSave(){mcsSS('mcs_poll_iv',String(Math.round(mcsP.iv/1000)));mcsSS('mcs_poll_pause',mcsP.paused?'1':'0');}
function mcsPollSchedule(ms){clearTimeout(mcsP.timer);mcsP.timer=null;if(mcsP.paused||!mcsP.url)return;mcsP.timer=setTimeout(mcsPollRun,ms||mcsP.iv);}
function mcsPollInit(t,url){if(url)mcsP.url=url;if(mcsP.inited)return;mcsP.inited=true;var iv=t>0?t:5000,paused=!(t>0),si=mcsSS('mcs_poll_iv'),sp=mcsSS('mcs_poll_pause');if(si&&parseInt(si,10)>0)iv=parseInt(si,10)*1000;if(sp==='1')paused=true;else if(sp==='0')paused=false;if(iv<1000)iv=1000;if(iv>99000)iv=99000;mcsP.iv=iv;mcsP.paused=paused;mcsPollUI();mcsPollSchedule();}
function mcsPollStep(d){if(mcsP.paused)return;var s=Math.round(mcsP.iv/1000)+d;if(s<1)s=1;if(s>99)s=99;mcsP.iv=s*1000;mcsPollSave();mcsPollUI();mcsPollSchedule();}
function mcsPollToggle(){mcsP.paused=!mcsP.paused;mcsPollSave();mcsPollUI();if(mcsP.paused){clearTimeout(mcsP.timer);mcsP.timer=null;}else mcsPollFetch(true);}
function mcsPollLocked(){var s=window.getSelection?window.getSelection():null;if(s&&!s.isCollapsed&&s.anchorNode){var m=document.getElementById('mainDiv');if(m&&m.contains(s.anchorNode))return true;}return false;}
function mcsPollRun(){mcsP.timer=null;if(mcsP.paused||!mcsP.url)return;if(document.hidden||mcsPollLocked()){mcsPollSchedule();return;}mcsPollFetch(false);}
function mcsPollNow(url){if(url&&!mcsP.url)mcsP.url=url;mcsPollFetch(true);}
function mcsPollBeat(){var v=document.getElementById('pl_val');if(!v)return;v.classList.add('hb');setTimeout(function(){v.classList.remove('hb');},300);}
function mcsPollErr(e){var d=document.getElementById('pollerr');if(e){if(!d&&document.body){d=document.createElement('div');d.id='pollerr';d.textContent='POLLERR';d.title='Polling failed - retrying in 15 s';document.body.appendChild(d);}}else if(d&&d.parentNode)d.parentNode.removeChild(d);}
function mcsPollFetch(force){var url=mcsP.url;if(!url)return;if(mcsP.busy){if(force)mcsP.again=true;return;}mcsP.busy=true;var x=new XMLHttpRequest();mcsP.xhr=x;x.open('GET',url,true);x.timeout=15000;var done=function(ok){if(mcsP.xhr!==x)return;mcsP.busy=false;mcsP.xhr=null;var ag=mcsP.again;mcsP.again=false;if(ok){mcsPollErr(false);mcsPollBeat();mcsPollSchedule(ag?250:mcsP.iv);}else{mcsPollErr(true);mcsPollSchedule(15000);}};x.onload=function(){if(x.status==200){if(x.responseURL&&/\/login(\?|$)/.test(x.responseURL)){location.href='/login';return;}mcsPatch(x.responseText);done(true);}else done(false);};x.onerror=x.ontimeout=function(){done(false);};x.send(null);}
function mcsPatch(html){var d=document.getElementById('mainDiv');if(!d)return;var t=document.createElement('div');t.innerHTML=html;try{mcsMorphKids(d,t);}catch(e){d.innerHTML=html;}if(window.bindSortable)bindSortable();if(window.colorStatusRows)colorStatusRows();if(window.sortReapply)sortReapply();if(window.dbgSyncBtn)dbgSyncBtn();if(window.reopenDbg)reopenDbg();}
function mcsKey(n){return(n.nodeType==1&&n.id)?n.id:null;}
function mcsKeep(n){return n.nodeType==1&&(n.getAttribute('data-keep')!==null||(n.id&&(n.id.indexOf('dbgrow_')===0||n.id.indexOf('cwfeedbox_')===0)));}
function mcsSame(a,b){return a.nodeType===b.nodeType&&(a.nodeType!==1||a.nodeName===b.nodeName);}
function mcsAfter(a,prev){var x=prev?prev.nextSibling:a.firstChild;while(x&&mcsKeep(x))x=x.nextSibling;return x;}
function mcsMorphKids(a,b){var ac=Array.prototype.slice.call(a.childNodes),bc=Array.prototype.slice.call(b.childNodes),map={},pool=[],seen={},i,n,k,pi=0,prev=null,free=(typeof window.sortCol!=='number');for(i=0;i<ac.length;i++){n=ac[i];if(mcsKeep(n))continue;k=mcsKey(n);if(k)map[k]=n;else pool.push(n);}
for(i=0;i<bc.length;i++){var nb=bc[i],na=null;k=mcsKey(nb);if(k){na=map[k]||null;if(na)seen[k]=1;}else if(pi<pool.length&&mcsSame(pool[pi],nb)){na=pool[pi];pi++;}
if(na){mcsMorphNode(na,nb);if(free||!k){if(mcsAfter(a,prev)!==na){var dg=(k&&k.indexOf('Row')===0)?document.getElementById('dbgrow_'+k.substring(3)):null;a.insertBefore(na,mcsAfter(a,prev));if(dg&&dg.parentNode===a)a.insertBefore(dg,na.nextSibling);}}}
else{na=document.importNode(nb,true);a.insertBefore(na,mcsAfter(a,prev));}
prev=na;}
for(k in map){if(!seen[k]&&map[k].parentNode===a){a.removeChild(map[k]);if(k.indexOf('Row')===0){var od=document.getElementById('dbgrow_'+k.substring(3));if(od&&od.parentNode===a)a.removeChild(od);}}}
for(;pi<pool.length;pi++){if(pool[pi].parentNode===a)a.removeChild(pool[pi]);}}
function mcsMorphNode(a,b){if(a.nodeType!==1){if(a.nodeValue!==b.nodeValue)a.nodeValue=b.nodeValue;return;}var nm=a.nodeName;if(nm==='SCRIPT'||nm==='STYLE')return;if(a.id==='dbglog'&&window.dbgPaused)return;var foc=(document.activeElement===a),i,at,ov=a.getAttribute('value'),nv=b.getAttribute('value'),oc=a.hasAttribute('checked'),nc=b.hasAttribute('checked');
for(i=0;i<b.attributes.length;i++){at=b.attributes[i];if(a.getAttribute(at.name)!==at.value)a.setAttribute(at.name,at.value);}
for(i=a.attributes.length-1;i>=0;i--){at=a.attributes[i];if(!b.hasAttribute(at.name))a.removeAttribute(at.name);}
if(nm==='INPUT'&&!foc&&a.type!=='file'){if(ov!==nv)a.value=(nv===null)?'':nv;if(oc!==nc)a.checked=nc;}
if(nm==='TEXTAREA'&&foc)return;
mcsMorphKids(a,b);
if(nm==='SELECT'&&!foc){var so=b.querySelector('option[selected]');if(so&&a.value!==so.value)a.value=so.value;}}
function sortReapply(){var n=window.sortCol,a=window.sortAsc;if(typeof n!=='number'||!a)return;var ts=document.querySelectorAll('table.maintable');for(var k=0;k<ts.length;k++){var tb=ts[k].tBodies[0];if(!tb)continue;var rs=Array.prototype.filter.call(tb.rows,function(r){return r.id&&r.id.indexOf('Row')===0&&r.cells.length>n;});if(rs.length<2)continue;var last=rs[rs.length-1],anchor=last.nextSibling;while(anchor&&anchor.nodeType==1&&mcsKeep(anchor))anchor=anchor.nextSibling;rs.sort(function(x,y){var p=x.cells[n].textContent.trim(),q=y.cells[n].textContent.trim(),pn=parseFloat(p),qn=parseFloat(q);return isNaN(pn)||isNaN(qn)?(p>q?1:p<q?-1:0)*a:(pn-qn)*a;});rs.forEach(function(r){var dg=document.getElementById('dbgrow_'+r.id.substring(3));tb.insertBefore(r,anchor);if(dg&&dg.parentNode===tb)tb.insertBefore(dg,r.nextSibling);});}}
document.addEventListener('visibilitychange',function(){if(!document.hidden&&mcsP.url&&!mcsP.paused&&!mcsP.busy){clearTimeout(mcsP.timer);mcsPollRun();}});
document.addEventListener('DOMContentLoaded',function(){var t=document.querySelector('.pollctl');if(t&&!mcsP.url)t.style.display='none';});
