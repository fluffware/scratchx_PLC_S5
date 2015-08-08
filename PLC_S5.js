(function(ext) {
    // Connect to I/O server
    var  io = new PLC_S5_comm('ws://localhost:28280');
    // Cleanup function when the extension is unloaded
    ext._shutdown = function() {
	io.disconnect();
    };

    // Status reporting code
    // Use this to report missing hardware, plugin or unsupported browser
    ext._getStatus = function() {
        return {status: 2, msg: 'Ready'};
    };
    
    ext.set_output = function(addr, bits)
    {
	io.dout(addr, bits);
    }
    
    ext.set_output_bit = function(addr, bit, state)
    {
	io.dout(int(addr),state ? 0xff : 0x00, (1<<int(bit)));
    }


    // Block and block menu descriptions
    var descriptor = {
        blocks: [
	    [' ', 'set output %n to %n', 'set_output',0,0],
	    [' ', 'set output %n bit %n to %n', 'set_output_bit',0,0,0],
        ]
    };
    // Register the extension
    ScratchExtensions.register('PLC S5 communication', descriptor, ext);
})({});
