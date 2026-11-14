// ztoit boot program: a minimal Toit system process, then the app.
//
// A bare app cannot be the boot program: print's service discovery is an RPC
// to the system process. So this installs toit's SystemServiceManager
// in-process, handles the system messages the full system/containers.toit
// would, installs a network service when the build has one, and calls the
// app's main. CMake copies the app in as app.toit and toit's system/services.toit
// and system/extensions/shared/network_base.toit beside it.
import encoding.base64
import .services show SystemServiceManager
import .network-base show NetworkServiceProviderBase
import system.services show ServiceProvider ServiceResource
import system.api.network show NetworkService
import .app as app

class SystemMessages_ implements SystemMessageHandler_:
  manager/SystemServiceManager? := null

  on-message type/int gid/int pid/int arguments/any -> none:
    if type == SYSTEM-TERMINATED_:
      if manager: manager.on-process-stop pid
    else if type == SYSTEM-TRACE_:
      // Decode on the host: toit decode --snapshot build/toit/program.snapshot <text>
      print_ "ztoit: trace:"
      write-on-stdout_ (base64.encode arguments) true

main:
  handler := SystemMessages_
  set-system-message-handler_ SYSTEM-TERMINATED_ handler
  set-system-message-handler_ SYSTEM-TRACE_ handler
  handler.manager = SystemServiceManager
  (ZephyrNetworkProvider).install
  app.main
  exit 0

// net.open on Zephyr: sockets run in the client process on the VM's own
// primitives (udp_zephyr.cc), so nothing is proxied, as on the host. Without a
// network stack in the build, the socket primitives fail at first use.
class ZephyrNetworkProvider extends NetworkServiceProviderBase:
  constructor:
    super "system/network/zephyr" --major=0 --minor=1

  connect client/int -> List:
    resource := ZephyrNetworkResource this client
    return [resource.serialize-for-rpc, NetworkService.PROXY-NONE, "thread"]

class ZephyrNetworkResource extends ServiceResource:
  constructor provider/ServiceProvider client/int:
    super provider client

  on-closed -> none:
