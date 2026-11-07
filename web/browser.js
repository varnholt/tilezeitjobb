// Browser file transfer and persistent workspace for the shared C++ editor.
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
  FS.mkdirTree('/imports'); FS.mkdirTree('/exports'); FS.mkdirTree('/projects');
  FS.mount(IDBFS, {}, '/projects');
  addRunDependency('workspace');
  FS.syncfs(true, function (error) {
    if (error) console.warn('Browser storage unavailable; use Save to download your work.', error);
    removeRunDependency('workspace');
  });
});
Module['importFile'] = async function (file, action) {
  if (!file) return;
  if (file.size > 32 * 1024 * 1024) { alert('Please choose a file smaller than 32 MB.'); return; }
  const name = file.name.replace(/[\/\\]/g, '_');
  const path = '/imports/' + name;
  FS.writeFile(path, new Uint8Array(await file.arrayBuffer()));
  Module.ccall('tzj_web_file', null, ['number', 'string'], [action, path]);
};
Module['pickFile'] = function (action) {
  const input = document.createElement('input'); input.type = 'file';
  input.accept = action === 1 || action === 7 ? '.tzj' : '.png,.bmp,.jpg,.jpeg,.tga';
  input.onchange = () => Module.importFile(input.files[0], action).catch(error => alert(error.message));
  input.click();
};
// Store-only ZIP: keeps the ROM and its matching raw assets in one browser download.
Module['createArchive'] = function (files) {
  const encoder = new TextEncoder(); const parts = [], directory = []; let offset = 0;
  const crc32 = data => {
    let crc = 0xffffffff;
    for (const byte of data) { crc ^= byte;
      for (let bit=0; bit<8; ++bit) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
    }
    return (crc ^ 0xffffffff) >>> 0;
  };
  for (const file of files) {
    const name=encoder.encode(file.name), data=file.data, crc=crc32(data);
    const local=new Uint8Array(30+name.length), h=new DataView(local.buffer);
    h.setUint32(0,0x04034b50,true); h.setUint16(4,20,true); h.setUint16(6,0x800,true);
    h.setUint16(12,33,true); h.setUint32(14,crc,true);
    h.setUint32(18,data.length,true); h.setUint32(22,data.length,true);
    h.setUint16(26,name.length,true); local.set(name,30);
    parts.push(local,data);
    const entry=new Uint8Array(46+name.length), d=new DataView(entry.buffer);
    d.setUint32(0,0x02014b50,true); d.setUint16(4,20,true); d.setUint16(6,20,true);
    d.setUint16(8,0x800,true); d.setUint16(14,33,true); d.setUint32(16,crc,true);
    d.setUint32(20,data.length,true); d.setUint32(24,data.length,true);
    d.setUint16(28,name.length,true); d.setUint32(42,offset,true); entry.set(name,46);
    directory.push(entry); offset+=local.length+data.length;
  }
  const length=directory.reduce((sum,p)=>sum+p.length,0);
  const end=new Uint8Array(22), e=new DataView(end.buffer);
  e.setUint32(0,0x06054b50,true); e.setUint16(8,files.length,true); e.setUint16(10,files.length,true);
  e.setUint32(12,length,true); e.setUint32(16,offset,true);
  return new Blob([...parts,...directory,end],{type:'application/zip'});
};
Module['downloadFile'] = function (path, bundle) {
  let blob, name=path.split('/').pop();
  if (bundle) {
      const extensions = bundle === 2 ? ['.nes','.chr','.nam','.pal','.oam','.json']
                       : bundle === 3 ? ['.png','.json']
                       : ['.nes','.chr','.nam','.pal','.oam','.h'];
      const files=extensions.map(extension => ({
      name:name+extension, data:FS.readFile(path+extension)
    }));
    blob=Module.createArchive(files); name+='.zip';
  } else blob=new Blob([FS.readFile(path)], {type:'application/octet-stream'});
  const url=URL.createObjectURL(blob), link=document.createElement('a');
  link.href=url; link.download=name; document.body.appendChild(link); link.click(); link.remove();
  setTimeout(()=>URL.revokeObjectURL(url),30000);
};
Module['toggleFullscreen'] = function () {
  if(document.fullscreenElement) document.exitFullscreen();
  else document.getElementById('studio').requestFullscreen().catch(error=>console.warn(error));
};
window.addEventListener('beforeunload', event => {
  if(Module.ready && Module.ccall('tzj_web_unsaved','number',[],[])) {
    event.preventDefault(); event.returnValue='';
  }
});
