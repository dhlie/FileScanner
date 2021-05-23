package d.hl.filescan

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import android.util.Log
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity

/**
 *
 * Author: duanhl
 * Create: 5/15/21 8:52 AM
 * Description:
 *
 */
class PermissionHelper(private val activity: FragmentActivity) {

    private lateinit var perms: List<String>

    private var onRationale: ((perms: List<String>, consumer: RationaleConsumer) -> Unit)? = null

    private var onGranted: (() -> Unit)? = null

    private var onDenied: ((perms: List<String>?, noAskAgainPerms: List<String>?) -> Unit)? = null

    private var intent: Intent? = null

    private var intentCallback: (() -> Unit)? = null

    private var rationaleConsumer: RationaleConsumer? = null
    private var permissionFragment: Fragment? = null

    fun permission(vararg perms: String): PermissionHelper {
        val requiredPerms = ArrayList<String>(perms.size)
        perms.forEach { perm ->
            if (ContextCompat.checkSelfPermission(activity, perm) != PackageManager.PERMISSION_GRANTED) {
                requiredPerms.add(perm)
            }
        }
        this.perms = requiredPerms
        return this
    }

    /**
     * 用户拒绝过权限, 向用户解释为什么需要相关权限
     * @param perms 拒绝过的权限
     */
    fun rationale(onRationale: (perms: List<String>, consumer: RationaleConsumer) -> Unit): PermissionHelper {
        this.onRationale = onRationale
        rationaleConsumer = RationaleConsumer()
        return this
    }

    /**
     * 申请的所有权限都被授权了
     */
    fun onGranted(callback: () -> Unit): PermissionHelper {
        onGranted = callback
        return this
    }

    /**
     * 被拒绝授权的权限
     * @param deniedPerms 拒绝的权限
     * @param noAskAgainPerms 不再询问的权限,需要到设置中打开
     */
    fun onDenied(callback: (deniedPerms: List<String>?, noAskAgainPerms: List<String>?) -> Unit): PermissionHelper {
        onDenied = callback
        return this
    }

    /**
     * 特殊权限 intent, 已有权限时返回 null(不会打开设置页面,直接回调 onIntentResult)
     */
    fun intentOrNull(block: () -> Intent?): PermissionHelper {
        val intent = block.invoke()
        this.intent = intent ?: Intent(INTENT_ACTION_PERMISSION_GRANTED)
        return this
    }

    /**
     * 特殊权限回调
     */
    fun onIntentResult(callback: () -> Unit): PermissionHelper {
        intentCallback = callback
        return this
    }

    /**
     * 开始处理权限请求, 特殊权限和普通权限需要分开请求
     */
    fun start() {
        // 请求特殊权限
        if (intent != null) {
            if (intent?.action == INTENT_ACTION_PERMISSION_GRANTED) {
                intentCallback?.invoke()
            } else {
                request()
            }
            return
        }

        // 请求普通权限
        if (!::perms.isInitialized) {
            throw RuntimeException("please invoke permission(...) declare required permissions")
        }

        if (perms.isEmpty()) {
            if (DEBUG) log("onAllGranted")
            onGranted?.invoke()
            return
        }

        if (onRationale != null) {
            rationaleConsumer!!.proceed()
        } else {
            request()
        }
    }

    private fun request() {
        permissionFragment = PermissionHelperFragment().apply { permissionHelper = this@PermissionHelper }
        activity.supportFragmentManager
            .beginTransaction()
            .add(permissionFragment!!, "PermissionHelperFragment")
            .commitAllowingStateLoss()
    }

    private fun done() {
        permissionFragment?.let {
            activity.supportFragmentManager
                .beginTransaction()
                .remove(it)
                .commitAllowingStateLoss()
        }
    }

    inner class RationaleConsumer {

        fun deny() {}

        fun accept() {
            request()
        }

        internal fun proceed() {

            var shouldRationalePerms: ArrayList<String>? = null
            for (perm in perms) {
                if (activity.shouldShowRequestPermissionRationale(perm)) {
                    if (shouldRationalePerms == null) {
                        shouldRationalePerms = ArrayList(perms.size)
                    }
                    shouldRationalePerms.add(perm)
                }
            }

            if (shouldRationalePerms.isNullOrEmpty()) {
                request()
            } else {
                onRationale!!.invoke(shouldRationalePerms, this)

                if (DEBUG) {
                    val list = shouldRationalePerms.map { perm -> permissionToText(perm) }
                    log("onRationale${list.toListString()}")
                }
            }
        }
    }

    class PermissionHelperFragment : Fragment() {

        lateinit var permissionHelper: PermissionHelper

        override fun onCreate(savedInstanceState: Bundle?) {
            super.onCreate(savedInstanceState)
            if (DEBUG) log("onCreate")

            if (permissionHelper.intent != null) {
                startActivityForResult(permissionHelper.intent, REQUEST_CODE)
            } else {
                requestPermissions(permissionHelper.perms.toTypedArray(), REQUEST_CODE)
            }
        }

        override fun onDestroy() {
            super.onDestroy()
            if (DEBUG) log("onDestroy")
        }

        override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
            super.onActivityResult(requestCode, resultCode, data)
            if (requestCode == REQUEST_CODE) {
                permissionHelper.done()
                permissionHelper.intentCallback?.invoke()
            }
        }

        override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
            if (requestCode == REQUEST_CODE) {
                permissionHelper.done()
                if (grantResults.isEmpty()) {
                    return
                }

                var deniedPerms: ArrayList<String>? = null
                var noAskAgainPerms: ArrayList<String>? = null
                for (i in grantResults.indices) {
                    if (grantResults[i] != PackageManager.PERMISSION_GRANTED) {
                        if (!shouldShowRequestPermissionRationale(permissions[i])) {
                            if (noAskAgainPerms == null) {
                                noAskAgainPerms = ArrayList(grantResults.size)
                            }
                            noAskAgainPerms.add(permissions[i])
                        } else {
                            if (deniedPerms == null) {
                                deniedPerms = ArrayList(grantResults.size)
                            }
                            deniedPerms.add(permissions[i])
                        }
                    }
                }

                if (deniedPerms.isNullOrEmpty() && noAskAgainPerms.isNullOrEmpty()) {
                    permissionHelper.onGranted?.invoke()
                    if (DEBUG) log("onAllGranted")
                } else {
                    permissionHelper.onDenied?.invoke(deniedPerms, noAskAgainPerms)
                    if (DEBUG) {
                        val list = deniedPerms?.map { perm -> permissionToText(perm) }
                        val noAskAgainList = noAskAgainPerms?.map { perm -> permissionToText(perm) }
                        log("onDenied:${list?.toListString()}, noAskAgain:${noAskAgainList?.toListString()}")
                    }
                }
            }
        }
    }

    companion object {
        private const val REQUEST_CODE = 0x41F8
        private const val INTENT_ACTION_PERMISSION_GRANTED = "permission.action.GRANTED"
        private const val DEBUG = true

        fun with(activity: FragmentActivity): PermissionHelper {
            return PermissionHelper(activity)
        }

        fun toAppSetting(context: Context) {
            val intent = Intent(
                Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                Uri.parse("package:${context.packageName}")
            )
            context.startActivity(intent)
        }

        fun log(msg: String) {
            if (DEBUG) {
                Log.i("PermissionHelper", msg)
            }
        }

        fun permissionToText(perm: String): String {
            return when (perm) {
                Manifest.permission.CAMERA -> "相机"
                Manifest.permission.ACCESS_FINE_LOCATION -> "定位"
                Manifest.permission.READ_EXTERNAL_STORAGE -> "读取存储卡"
                Manifest.permission.WRITE_EXTERNAL_STORAGE -> "写入存储卡"
                else -> perm
            }
        }

        private fun Iterable<String>.toListString(): String {
            val builder = StringBuilder("[")
            for (t in this) {
                builder.append(t).append(",")
            }
            builder.deleteCharAt(builder.length - 1)
            builder.append("]")
            return builder.toString()
        }
    }

}