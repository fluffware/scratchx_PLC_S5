

var dout_state = [];

var ain_elements = [];
var start_time;
function page_onload()
{
    io = new PLC_S5_comm('ws://localhost:28280');
    io.onDout = doutHandler;
}


function page_onunload()
{
    io.disconnect();
    clearTimeout(din_timer);
    clearTimeout(dout_timer);
}


function doutHandler(addr, bits)
{
    console.log("Latency: " + (Date.now() - start_time));
    dout_state[addr] = bits;
    toggleDout(32,255);
    
}

function toggleDout(addr, mask)
{
    io.dout(addr, ~dout_state[addr], mask);
    start_time = Date.now();
    return false;
}
