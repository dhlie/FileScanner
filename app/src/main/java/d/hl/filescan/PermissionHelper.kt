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

    /**
     * 用户拒绝过权限, 需要向用户解释为什么需要相关权限
     */
    private var onRationale: ((perms: List<String>, consumer: RationaleConsumer) -> Unit)? = null

    /**
     * 申请的所有权限都被授权了
     */
    private var onAllGranted: (() -> Unit)? = null

    /**
     * 被拒绝授权的权限
     */
    private var onDenied: ((perms: List<String>) -> Unit)? = null

    /**
     * 用户选择了不在询问的权限
     */
    private var onNoAskAgain: ((perms: List<String>) -> Unit)? = null
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

    fun rationale(onRationale: (perms: List<String>, consumer: RationaleConsumer) -> Unit): PermissionHelper {
        this.onRationale = onRationale
        rationaleConsumer = RationaleConsumer()
        return this
    }

    fun onAllGranted(callback: () -> Unit): PermissionHelper {
        onAllGranted = callback
        return this
    }

    fun onDenied(callback: (perms: List<String>) -> Unit): PermissionHelper {
        onDenied = callback
        return this
    }

    fun onNoAskAgain(callback: (perms: List<String>) -> Unit): PermissionHelper {
        onNoAskAgain = callback
        return this
    }

    fun start() {
        if (!::perms.isInitialized) {
            throw RuntimeException("please invoke permission(...) declare required permissions")
        }

        if (perms.isEmpty()) {
            if (DEBUG) log("onAllGranted")
            onAllGranted?.invoke()
            return
        }

        if (onRationale != null) {
            rationaleConsumer!!.proceed()
        } else {
            request()
        }
    }

    private fun request() {
        permissionFragment = PermissionFragment().apply { permissionHelper = this@PermissionHelper }
        activity.supportFragmentManager
            .beginTransaction()
            .add(permissionFragment!!, null)
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

    class PermissionFragment : Fragment() {

        lateinit var permissionHelper: PermissionHelper

        override fun onCreate(savedInstanceState: Bundle?) {
            super.onCreate(savedInstanceState)
            if (DEBUG) log("onCreate")
            requestPermissions(permissionHelper.perms.toTypedArray(), REQUEST_CODE)
        }

        override fun onDestroy() {
            super.onDestroy()
            if (DEBUG) log("onDestroy")
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
                    permissionHelper.onAllGranted?.invoke()
                    if (DEBUG) log("onAllGranted")
                } else {
                    deniedPerms?.let {
                        permissionHelper.onDenied?.invoke(it)
                        if (DEBUG) {
                            val list = it.map { perm -> permissionToText(perm) }
                            log("onDenied${list.toListString()}")
                        }
                    }
                    noAskAgainPerms?.let {
                        permissionHelper.onNoAskAgain?.invoke(it)
                        if (DEBUG) {
                            val list = it.map { perm -> permissionToText(perm) }
                            log("onNoAskAgain${list.toListString()}")
                        }
                    }
                }
            }
        }
    }

    companion object {
        private const val REQUEST_CODE = 100
        private const val DEBUG = true

        fun with(activity: FragmentActivity): PermissionHelper {
            return PermissionHelper(activity)
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