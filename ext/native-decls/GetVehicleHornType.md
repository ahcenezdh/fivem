---
ns: CFX
apiset: server
---
## GET_VEHICLE_HORN_TYPE

```c
Hash GET_VEHICLE_HORN_TYPE(Vehicle vehicle);
```

This native is a getter for the client-side native `START_VEHICLE_HORN`, which allows you to return the horn type of the vehicle.

**Note**: This native only gets the hash value set with `START_VEHICLE_HORN`. If a wrong hash is passed into `START_VEHICLE_HORN`, it will return this wrong hash.

```c
enum eHornTypes
{
    NORMAL = 1330140148,
    HELDDOWN = -2087385909,
    AGGRESSIVE = -92810745
}

## Parameters
* **vehicle**: The vehicle to check the horn type.

## Return value
The vehicle horn type hash. Returns 0 if the vehicle does not have a horn type set.