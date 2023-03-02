using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class GodCamera : MonoBehaviour
{
    public float moveSpeed = 10f;
    public float rotateSpeed = 100f;
    public float panSpeed = 50f;
    public float freeLookSensitivity = 3f;
    public bool isActive = true;

    public float shiftSpeedMultiplier = 2f;
    private float activeSpeedMultiplier = 1f;

    // Update is called once per frame
    void Update()
    {
        if (!isActive) return;

        if (Input.GetKey(KeyCode.LeftShift)) {
            activeSpeedMultiplier = shiftSpeedMultiplier;
        } else {
            activeSpeedMultiplier = 1f;
        }

        //look around with mouse if left mouse button is pressed
        if (Input.GetMouseButton(0)) {
            //look around as if FPS camera
            float newRotationX = transform.localEulerAngles.x - Input.GetAxis("Mouse Y") * freeLookSensitivity;
            float newRotationY = transform.localEulerAngles.y + Input.GetAxis("Mouse X") * freeLookSensitivity;
            transform.localEulerAngles = new Vector3(newRotationX, newRotationY, 0f);
        }

        //pan camera with mouse if middle mouse button is pressed
        if (Input.GetMouseButton(2)) {
            transform.Translate(Vector3.right * Input.GetAxis("Mouse X") * panSpeed * activeSpeedMultiplier * Time.deltaTime);
            transform.Translate(Vector3.up * Input.GetAxis("Mouse Y") * panSpeed * activeSpeedMultiplier * Time.deltaTime, Space.World);
        }

        //move camera per keyboard (horizontal) inputmanager
        if (Input.GetAxis("Horizontal") != 0) {
            transform.Translate(Vector3.right * Input.GetAxis("Horizontal") * moveSpeed * activeSpeedMultiplier * Time.deltaTime);
        }

        //move camera per keyboard (vertical) inputmanager
        if (Input.GetAxis("Vertical") != 0) {
            transform.Translate(Vector3.forward * Input.GetAxis("Vertical") * moveSpeed * activeSpeedMultiplier * Time.deltaTime);
        }
    }
}
