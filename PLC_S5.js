(function(ext) {
    // Cleanup function when the extension is unloaded
    ext._shutdown = function() {};

    // Status reporting code
    // Use this to report missing hardware, plugin or unsupported browser
    ext._getStatus = function() {
        return {status: 2, msg: 'Ready'};
    };
    
    ext.set_output = function()
    {
    }
    
    // Block and block menu descriptions
    var descriptor = {
        blocks: [
	    [' ', 'set output %n to %n', 'set_output',0,0],
        ]
    };

    // Register the extension
    ScratchExtensions.register('PLC S5 communication', descriptor, ext);
})({});
