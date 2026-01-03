import org.zeromq.ZMQ;
import org.zeromq.ZContext;

public class TestZeroMQ {
    public static void main(String[] args) {
        // Create a ZeroMQ context
        ZContext context = new ZContext();
        
        // Create a publisher socket
        ZMQ.Socket publisher = context.createSocket(ZMQ.PUB);
        
        // Connect to a test endpoint
        publisher.connect("tcp://localhost:2001");
        
        // Send a test message
        publisher.send("Hello, ZeroMQ!");
        
        // Clean up
        publisher.close();
        context.close();
        
        System.out.println("Message sent successfully!");
    }
}