var fs = require('fs');
const util = require('util');
var exec = util.promisify(require('child_process').exec);

var totalCount = 1000;
var smartCodes = new Array();

async function generateCodes() {
    if(fs.existsSync('smartCodes.txt') == false) {
        var smartCodes = new Array();
        for(var idx = 0; idx < totalCount; idx++) {
            count = idx;
            var smartCode = 'pragma solidity ^0.4.0;\r\n' +
                'contract TestB{\r\n' +
                '   constructor() payable public{\r\n' +
                '   }\r\n' +
                '   function return_const() public returns (uint){\r\n' +
                '       return ' + idx + ';\r\n' +
                '   }\r\n' +
                '}\r\n';

            fs.writeFileSync('test.sol', smartCode);
            var {error, stdout, stderr} = await exec('solc.exe --bin test.sol');
            var regex = /(.)+/g;
            var matched = stdout.match(regex);
            smartCodes.push(matched[2]);
            //console.log(matched[2]);
        }
        var codes = '';
        for(var i = 0; i < smartCodes.length; i++) {
            //console.log(smartCodes[i]);
            codes += smartCodes[i];
            if(i < smartCodes.length - 1) {
                codes += ';';
            }
        }

        fs.writeFileSync('smartCodes.txt', codes);
    }

    var {stdout, stderr} = await exec('chainsqld_classic.exe --unittest="VM" --unittest-arg="file=smartCodes.txt"');
    if (error) {
        console.log(error);
    }

    if (stdout) {
        console.log(stdout);
    }

    if (stderr) {
        console.log(stderr);
    }
}

generateCodes();

