package com.nocturne.bmwassistant

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.RadioButton
import android.widget.Spinner
import androidx.fragment.app.Fragment
import com.google.android.material.checkbox.MaterialCheckBox
import com.google.android.material.switchmaterial.SwitchMaterial
import com.google.android.material.textfield.TextInputEditText

class SettingsFragment : Fragment() {

    private val carModelKeys = arrayOf("e39", "e39_fl", "e46", "e46_fl", "e53", "e53_fl", "e38")

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_settings, container, false)
    }

        override fun onPause() {
        super.onPause()
        (activity as? MainActivity)?.let { act ->
            view?.findViewById<TextInputEditText>(R.id.settings_welcome_cluster)?.text?.toString()?.let { act.setWelcomeClusterPref(it) }
            view?.findViewById<TextInputEditText>(R.id.settings_shift_rpm)?.text?.toString()?.toIntOrNull()?.let { act.setShiftRpmPref(it) }
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val activity = activity as? MainActivity ?: return

        val themeGroup = view.findViewById<android.widget.RadioGroup>(R.id.settings_theme_group)
        val themeLight = view.findViewById<RadioButton>(R.id.theme_light)
        val themeDark = view.findViewById<RadioButton>(R.id.theme_dark)
        val themeSystem = view.findViewById<RadioButton>(R.id.theme_system)

        when (activity.getThemePref()) {
            "light" -> themeLight.isChecked = true
            "dark" -> themeDark.isChecked = true
            else -> themeSystem.isChecked = true
        }

        themeGroup.setOnCheckedChangeListener { _, checkedId ->
            val theme = when (checkedId) {
                R.id.theme_light -> "light"
                R.id.theme_dark -> "dark"
                else -> "system"
            }
            activity.setThemePref(theme)
            activity.recreate()
        }

        val autoConnect = view.findViewById<SwitchMaterial>(R.id.settings_auto_connect)
        autoConnect.isChecked = activity.getAutoConnectPref()
        autoConnect.setOnCheckedChangeListener { _, isChecked ->
            activity.setAutoConnectPref(isChecked)
        }

        val welcomeCluster = view.findViewById<TextInputEditText>(R.id.settings_welcome_cluster)
        welcomeCluster.setText(activity.getWelcomeClusterPref())
        welcomeCluster.setOnFocusChangeListener { _, hasFocus -> if (!hasFocus) activity.setWelcomeClusterPref(welcomeCluster.text?.toString() ?: "") }

        val shiftRpm = view.findViewById<TextInputEditText>(R.id.settings_shift_rpm)
        shiftRpm.setText(activity.getShiftRpmPref().toString())
        shiftRpm.setOnFocusChangeListener { _, hasFocus ->
            if (!hasFocus) activity.setShiftRpmPref(shiftRpm.text?.toString()?.toIntOrNull() ?: 5500)
        }

        val showTrackCluster = view.findViewById<SwitchMaterial>(R.id.settings_show_track_cluster)
        showTrackCluster.isChecked = activity.getShowTrackClusterPref()
        showTrackCluster.setOnCheckedChangeListener { _, isChecked -> activity.setShowTrackClusterPref(isChecked) }

        val confirmDangerous = view.findViewById<SwitchMaterial>(R.id.settings_confirm_dangerous)
        confirmDangerous.isChecked = activity.getConfirmDangerousPref()
        confirmDangerous.setOnCheckedChangeListener { _, isChecked -> activity.setConfirmDangerousPref(isChecked) }

        val leaveCloseWindows = view.findViewById<MaterialCheckBox>(R.id.scenario_leave_close_windows)
        leaveCloseWindows.isChecked = activity.getScenarioLeaveCloseWindows()
        leaveCloseWindows.setOnCheckedChangeListener { _, isChecked -> activity.setScenarioLeaveCloseWindows(isChecked) }

        val leaveLock = view.findViewById<MaterialCheckBox>(R.id.scenario_leave_lock)
        leaveLock.isChecked = activity.getScenarioLeaveLock()
        leaveLock.setOnCheckedChangeListener { _, isChecked -> activity.setScenarioLeaveLock(isChecked) }

        val leaveFollowMeHome = view.findViewById<MaterialCheckBox>(R.id.scenario_leave_follow_me_home)
        leaveFollowMeHome.isChecked = activity.getScenarioLeaveFollowMeHome()
        leaveFollowMeHome.setOnCheckedChangeListener { _, isChecked -> activity.setScenarioLeaveFollowMeHome(isChecked) }

        val leaveGoodbyeLights = view.findViewById<MaterialCheckBox>(R.id.scenario_leave_goodbye_lights)
        leaveGoodbyeLights.isChecked = activity.getScenarioLeaveGoodbyeLights()
        leaveGoodbyeLights.setOnCheckedChangeListener { _, isChecked -> activity.setScenarioLeaveGoodbyeLights(isChecked) }

        val carModelSpinner = view.findViewById<Spinner>(R.id.settings_car_model)
        val labels = carModelKeys.map { getString(when (it) {
            "e39" -> R.string.car_model_e39
            "e39_fl" -> R.string.car_model_e39_fl
            "e46" -> R.string.car_model_e46
            "e46_fl" -> R.string.car_model_e46_fl
            "e53" -> R.string.car_model_e53
            "e53_fl" -> R.string.car_model_e53_fl
            "e38" -> R.string.car_model_e38
            else -> R.string.car_model_e39_fl
        }) }
        carModelSpinner.adapter = ArrayAdapter(requireContext(), android.R.layout.simple_spinner_item, labels).apply {
            setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        }
        val current = activity.getCarModelPref()
        val idx = carModelKeys.indexOf(current).coerceAtLeast(0)
        carModelSpinner.setSelection(idx)
        carModelSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                activity.setCarModelPref(carModelKeys[position])
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
    }
}
