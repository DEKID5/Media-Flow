import fs from 'fs';

const filePath = 'src/components/OperatorDashboard.tsx';
let lines = fs.readFileSync(filePath, 'utf-8').split('\n');

// Specific cleaning
// We want to remove the garbage at 945-948 and 1171 (based on the previous view_file which had line numbers)
// BUT line numbers shift when deleting.
// Garbage found in view_file (1-indexed):
// 945:                      </div>
// 946: 
// 947:                      <div className="space-y-1 pb-2">
// 948:                        <label className="text-[9px] font-black text-white/20 uppercase tracking-widest ml-1">File Path (Optional)</label>
// 1171: iv>

// We'll search for the exact content strings to be safe.
const garbageStrings = [
    '                     </div>',
    ' ',
    '                     <div className="space-y-1 pb-2">',
    '                       <label className="text-[9px] font-black text-white/20 uppercase tracking-widest ml-1">File Path (Optional)</label>',
    'iv>',
    'on>'
];

let newLines = lines.filter(line => {
    const trimmed = line.trim();
    if (trimmed === 'iv>' || trimmed === 'on>') return false;
    if (line === '                     </div>' && lines[lines.indexOf(line)-1]?.includes('</section>')) return false;
    return true;
});

// Since the filter might be too aggressive, I'll do a simple slice if I can pinpoint it.
// Let's just rewrite the problematic area by finding anchors.

const content = fs.readFileSync(filePath, 'utf-8');

// The most broken part is the transition between sections and the end of BGM.
// I'll try a regex replace for the known messes.

let fixed = content
    .replace(/<\/section>\s+<\/div>\s+<div className="space-y-1 pb-2">[\s\S]+?<section/g, '</section>\n\n           <section')
    .replace(/<\/section>\s+on>\s+<section/g, '</section>\n\n           <section')
    .replace(/<\/div>\s+iv>[\s\S]+?<\/div>/g, '</div>')
    .replace(/<\/div>\s+<\/div>\s+iv>/g, '</div>\n           </div>');

fs.writeFileSync(filePath, fixed);
console.log('File patched');
