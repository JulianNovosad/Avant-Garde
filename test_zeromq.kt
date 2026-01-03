import org.zeromq.ZMQ
import org.zeromq.ZContext

fun main() {
    // Create a ZeroMQ context
    val context = ZContext()
    
    // Create a publisher socket
    val publisher = context.createSocket(ZMQ.PUB)
    
    // Connect to a test endpoint
    publisher.connect("tcp://localhost:2001")
    
    // Send a test message
    publisher.send("Hello, ZeroMQ!")
    
    // Clean up
    publisher.close()
    context.close()
    
    println("Message sent successfully!")
}