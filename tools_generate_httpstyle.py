# -*- coding: utf-8 -*-
# Gera src/httpstyle.c a partir de Configs/multics.css + JS embutido
import os

BASE = os.path.dirname(os.path.abspath(__file__))
css_path = os.path.join(BASE, 'Configs', 'multics.css')
out_path = os.path.join(BASE, 'src', 'httpstyle.c')

css = open(css_path, encoding='utf-8').read()

# versao da build (do common.h) para o footer
version = '1.0'
try:
    common = open(os.path.join(BASE, 'src', 'common.h'), encoding='utf-8', errors='replace').read()
    import re
    m = re.search(r'#define\s+VERSION_STR\s+"([^"]+)"', common)
    if m:
        version = m.group(1)
except Exception:
    pass

js = r'''function imgrequest( url, el )
{
	var httpRequest;
	try { httpRequest = new XMLHttpRequest(); }
	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }
	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }
	if ( typeof(el)!='undefined' ) {
		el.onclick = null;
		el.style.opacity = '0.7';
		httpRequest.onreadystatechange = function()
		{
			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';
		}
	}
	httpRequest.open('GET', url, true);
	httpRequest.send(null);
}
function sortTable(el,n){var t=el.closest?el.closest('table'):null;if(!t)return;var tb=t.tBodies[0]||t;var r=t.tHead?t.tHead.rows[0].cells:t.rows[0].cells;var h=r[n];if(!h)return;var b=Array.from(tb.rows);var a=window['sortAsc']=(window['sortCol']==n)?-window['sortAsc']:1;window['sortCol']=n;b.sort(function(x,y){var p=x.cells[n].textContent.trim(),q=y.cells[n].textContent.trim();var pn=parseFloat(p),qn=parseFloat(q);return isNaN(pn)||isNaN(qn)?(p>q?1:p<q?-1:0)*a:(pn-qn)*a});b.forEach(function(r){tb.appendChild(r)});}
function bindSortable(){var ts=document.querySelectorAll('table.maintable');for(var k=0;k<ts.length;k++){(function(t){var ths=t.querySelectorAll('th');for(var i=0;i<ths.length;i++){(function(n){ths[n].style.cursor='pointer';ths[n].title='Click to sort';ths[n].onclick=function(){sortTable(this,n);};})(i);}})(ts[k]);}}
function fetchDebugLog(){var d=document.getElementById('dbglog');if(!d)return;var x=new XMLHttpRequest();x.open('GET','/debug?action=log',true);x.onreadystatechange=function(){if(x.readyState==4&&x.status==200){d.innerHTML=x.responseText;}};x.send(null);}
function setDebugFilter(v){imgrequest('/debug?action=debug&value='+v);setTimeout(fetchDebugLog,150);}
function toggleDbgRow(id,url){var r=document.getElementById('dbgrow_'+id);if(r){r.parentNode.removeChild(r);return;}var tr=document.getElementById('Row'+id);if(!tr)return;var x=new XMLHttpRequest();x.open('GET',url,true);x.onreadystatechange=function(){if(x.readyState==4&&x.status==200){var t=document.createElement('tr');t.id='dbgrow_'+id;t.className='dbgrow';var c=document.createElement('td');c.colSpan=99;c.innerHTML=x.responseText;t.appendChild(c);tr.parentNode.insertBefore(t,tr.nextSibling);}};x.send(null);}
function colorStatusRows(){var tds=document.querySelectorAll('table.maintable td.offline, table.maintable td.online, table.maintable td.busy');for(var i=0;i<tds.length;i++){var tr=tds[i].parentNode;if(!tr)continue;tr.classList.remove('row-offline');tr.classList.remove('row-online');tr.classList.remove('row-busy');if(tds[i].classList.contains('offline'))tr.classList.add('row-offline');else if(tds[i].classList.contains('online'))tr.classList.add('row-online');else tr.classList.add('row-busy');}}
setInterval(colorStatusRows,2000);
function applyTheme(t){if(t==='light'){document.body.classList.add('light-mode');document.documentElement.classList.add('light-mode');}else{document.body.classList.remove('light-mode');document.documentElement.classList.remove('light-mode');}var btn=document.getElementById('themeToggle');if(btn){btn.textContent=(t==='light')?'Dark':'Light';}}
function toggleTheme(){var cur=(document.body.classList.contains('light-mode'))?'light':'dark';var t=(cur==='light')?'dark':'light';applyTheme(t);try{localStorage.setItem('theme',t);}catch(e){}}
document.addEventListener('DOMContentLoaded',function(){var t='light';try{t=localStorage.getItem('theme')||'light';}catch(e){}applyTheme(t);bindSortable();colorStatusRows();if(document.getElementById('dbglog'))setInterval(fetchDebugLog,2000);var d=document.getElementById('mainDiv');if(d){var f=document.createElement('div');f.className='home-footer';f.innerHTML="<span class='hf-ver'>MultiCS r1000 v%s - All Rights Reserved - Sharillas@2026</span>";if(d.parentNode){d.parentNode.insertBefore(f,d.nextSibling);}}});
''' % version

def c_string(s):
    s = s.replace('\\', '\\\\').replace('"', '\\"')
    s = s.replace('\n', '\\n')
    return '"' + s + '"'

out = 'char style_css[] = %s;\nchar java_file[] = %s;\n' % (c_string(css), c_string(js))
open(out_path, 'w', encoding='utf-8', newline='').write(out)
print('httpstyle.c gerado:', len(out), 'bytes')
